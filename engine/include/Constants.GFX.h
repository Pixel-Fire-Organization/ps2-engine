#pragma once

#define GFX_SCREEN_REGION_PAL 0
#define GFX_SCREEN_REGION_NTSC 1

// pixels per page (width) in GS
#define GFX_GS_PAGE_WIDTH_PSM32 64

// pixels per page (height) in GS
#define GFX_GS_PAGE_HEIGHT_PSM32 32

#define GFX_GS_PSM_CT32 0u

#define GFX_MAX_TEXTURE_GS_PAGES 64

// Total GS VRAM pages available for PSM32 textures across all Raylib texture slots.
#define GFX_GS_TEXTURE_PAGE_BUDGET 264

#define GFX_MAX_TEXTURE_WIDTH 512
#define GFX_MAX_TEXTURE_HEIGHT 512
#define GFX_SCREEN_PAL_WIDTH 640
#define GFX_SCREEN_PAL_HEIGHT 512
#define GFX_SCREEN_NTSC_WIDTH 640
#define GFX_SCREEN_NTSC_HEIGHT 448

#ifdef REGION_PAL
    #define GFX_SCREEN_WIDTH GFX_SCREEN_PAL_WIDTH
    #define GFX_SCREEN_HEIGHT GFX_SCREEN_PAL_HEIGHT
    #define GFX_SCREEN_REGION GFX_SCREEN_REGION_PAL
    #define GFX_SCREEN_REGION_STR "PAL"
#else
    #define GFX_SCREEN_WIDTH GFX_SCREEN_NTSC_WIDTH
    #define GFX_SCREEN_HEIGHT GFX_SCREEN_NTSC_HEIGHT
    #define GFX_SCREEN_REGION GFX_SCREEN_REGION_NTSC
    #define GFX_SCREEN_REGION_STR "NTSC"
#endif

#define GFX_MAX_DRAW_LIST_LENGTH 1024

// ---------------------------------------------------------------------------
// ps2gl VIF1 DMA packet budget  (SHARED across all subsystems)
// ---------------------------------------------------------------------------
// ps2gl allocates a fixed-size main DMA frame packet:
//   CGLContext::CurPacket → kDmaPacketMaxQwordLength = 65000 qwords (1 MB).
// CGLContext::GetVif1Packet() always returns *CurPacket, so EVERY rendering
// path that writes gl commands (glCallList, ImmGeomManager, rlgl batch) uses
// this same packet.
//
// Each glCallList that changes the modelview matrix (glTranslatef/glScalef)
// or color (glColor4f) triggers a full VU1 renderer context re-upload:
//   • 77 qwords VU1 context (matrices, lights, material, GIF tag)
//   • ~3 qwords DMA/VIF header overhead (CNT, STCYCL, FLUSH, MSCAL…)
//   • ~2 qwords DMA CALL tag to the pre-compiled geometry packet
//   Total: ~82 qwords per draw call written into CurPacket.
//
// Budget consumers per frame (all sharing GFX_DRAW_CALL_BUDGET):
//   1. RenderPrimitives  — up to GFX_DRAW_CALL_BUDGET primitives (~82q each)
//   2. RenderModels      — each model mesh DList call (~82q first mesh/model,
//                          ~2-3q for same-model subsequent meshes if color
//                          is not reset between them)
//   3. Skybox (rlgl)     — one cube, ~85q fixed overhead
//   4. UI / DrawGrid / text — rlgl batch, bounded lower overhead
//
// Theoretical max: floor(65000 / 82) = 792 draws/frame.
// GFX_DRAW_CALL_BUDGET = 640 leaves ~16 % headroom for the fixed-cost items
// in bullets 3-4 above.
//
// Exceeding it causes CurPacket to silently overflow in release builds
// (mErrorIf is a no-op), corrupting heap → bad VifCmd → TLB miss → crash.
#define GFX_PGL_MAIN_PACKET_QWORDS 65000 // kDmaPacketMaxQwordLength (ps2gl)
#define GFX_QWORDS_PER_DRAWCALL 82 // measured overhead per glCallList
#define GFX_DRAW_CALL_BUDGET 640 // safe combined limit (prims + model meshes)

// ---------------------------------------------------------------------------
// Model display list cache
// ---------------------------------------------------------------------------
// Each unique model resource can have up to GFX_MAX_MODEL_MESH_COUNT meshes
// compiled into ps2gl display lists, mirroring the primitive DList strategy.
//
// ps2gl CONSTRAINT: glDrawElements() is a hard mError() → indexed meshes
// (mesh.indices != nullptr) are NOT supported and are skipped at compile time.
// Use GenMesh* Raylib functions or export models with shared-index removal.
// pglDrawIndexedArrays() only handles unsigned-byte indices (< 256 vertices),
// which is too small for real models and is not used here.
//
// GFX_MAX_CACHED_MODELS limits the unique model resources that can have their
// DLists resident simultaneously.  Call ClearModelDListCache() on level unload.
#define GFX_MAX_MODEL_MESH_COUNT 8 // max meshes per model supported in cache
#define GFX_MAX_CACHED_MODELS 16 // max unique model DList cache entries

#define PRIMITIVE_CUBE_VERTEX_STRIDE 8
#define PRIMITIVE_VERTEX_STRIDE 8
#define PRIMITIVE_CUBE_VERTEX_COUNT 36
// Octahedron: 8 faces * 3 verts/face = 24 verts
#define PRIMITIVE_SPHERE_VERTEX_COUNT 336
// 4-sided cylinder: 4 side quads (2 tris each) + 4 top-cap + 4 bottom-cap tris
// = (8 + 4 + 4) tris * 3 = 48 verts
#define PRIMITIVE_CYLINDER_VERTEX_COUNT 144
