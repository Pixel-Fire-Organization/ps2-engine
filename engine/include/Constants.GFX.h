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
// ps2gl VIF1 DMA packet budget
// ---------------------------------------------------------------------------
// ps2gl allocates a fixed-size main DMA frame packet:
//   CGLContext::CurPacket → kDmaPacketMaxQwordLength = 65000 qwords (1 MB).
// Each glCallList that changes the modelview matrix or color (which happens
// every primitive because of glTranslatef + glColor4f) triggers a full
// VU1 renderer context upload via AddVu1RendererContext():
//   • 77 qwords VU1 context (matrices, lights, material, GIF tag)
//   • ~3 qwords DMA/VIF header overhead (CNT, STCYCL, FLUSH, MSCAL…)
//   • ~2 qwords DMA CALL tag to the pre-compiled geometry packet
//   Total: ~82 qwords per draw call written into CurPacket.
//
// Theoretical max: floor(65000 / 82) = 792 draws/frame.
// GFX_DRAW_CALL_BUDGET leaves ~16 % headroom for rlgl, DrawGrid, UI, text.
// Exceeding it causes CurPacket to silently overflow in release builds
// (mErrorIf is a no-op), corrupting heap → bad VifCmd → TLB miss → crash.
#define GFX_PGL_MAIN_PACKET_QWORDS 65000 // kDmaPacketMaxQwordLength (ps2gl)
#define GFX_QWORDS_PER_DRAWCALL 82 // measured overhead per glCallList
#define GFX_DRAW_CALL_BUDGET 640 // safe per-frame primitive limit

#define PRIMITIVE_CUBE_VERTEX_STRIDE 8
#define PRIMITIVE_VERTEX_STRIDE 8
#define PRIMITIVE_CUBE_VERTEX_COUNT 36
// Octahedron: 8 faces * 3 verts/face = 24 verts
#define PRIMITIVE_SPHERE_VERTEX_COUNT 336
// 4-sided cylinder: 4 side quads (2 tris each) + 4 top-cap + 4 bottom-cap tris
// = (8 + 4 + 4) tris * 3 = 48 verts
#define PRIMITIVE_CYLINDER_VERTEX_COUNT 144
