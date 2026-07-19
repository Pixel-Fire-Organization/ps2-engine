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

// Projection aspect ratio. The PS2 outputs its framebuffer (640x448 NTSC /
// 640x512 PAL) to a 4:3 display with NON-SQUARE pixels, so the perspective
// projection must use the DISPLAY aspect (4:3), not the framebuffer pixel ratio
// (640/512 = 1.25 would stretch geometry ~7% wide and differ between regions).
// (For a 16:9 mode this would become 16/9.)
#define GFX_DISPLAY_ASPECT (4.0f / 3.0f)

#define GFX_MAX_DRAW_LIST_LENGTH 1024

// Shared clip-plane distances — both backends must build the same frustum so
// CPU-side culling matches what the GS rasterizes.
#define GFX_NEAR_PLANE 0.1f
#define GFX_FAR_PLANE 1000.0f

// Fixed 3D camera slots. Exactly one is the active render camera per frame;
// scripts move/rotate slots on demand and select which one is active.
#define GFX_MAX_CAMERAS_3D 4

// ps2gl CurPacket is 65,000 qwords shared by all render paths. See docs/rendering/PS2GL_FUNCTIONS.md.
#define GFX_PGL_MAIN_PACKET_QWORDS 65000 // kDmaPacketMaxQwordLength (ps2gl)
#define GFX_QWORDS_PER_DRAWCALL 82 // per glCallList with state change
#define GFX_DRAW_CALL_BUDGET 720 // combined limit — primitives + model meshes

// GIFTAG renderer (direct GS packet path) per-frame budgets. Unlike the PS2GL
// path, vertices are transformed on the EE and written straight into a GIF/DMA
// packet, so the limit is packet capacity, not draw calls. The two geometry
// packets are double-buffered: while the GS drains frame N, the EE builds frame
// N+1 into the other packet. The renderer drops excess geometry loudly (never a
// silent DMA overrun).
//   Textured strip vertex = ST+RGBAQ+XYZ2 = 3 regs = 1.5 qw; 61440 qw ≈ 40k
//   textured verts, ~960 KB per packet. packet2 qword count is u16 (<= 65535).
#define GFX_GIFTAG_PACKET_QWORDS 61440 // per-packet DMA capacity (~960 KB)
#define GFX_GIFTAG_PACKET_BUFFERS 2 // double-buffered geometry packets
#define GFX_GIFTAG_MAX_VERTS 40000 // per-frame transformed-vertex cap (scratch bound)
#define GFX_GIFTAG_XFORM_BATCH 1024 // verts per VU0 batch transform (Stage B3)
#define GFX_GIFTAG_PACKET_MARGIN_QW 64 // headroom left free per packet

// glDrawElements is a hard mError() in ps2gl; indexed meshes are skipped.
#define GFX_MAX_MODEL_MESH_COUNT 8 // max meshes per model in DList cache
#define GFX_MAX_CACHED_MODELS 16 // max unique models resident at once

#define PRIMITIVE_CUBE_VERTEX_STRIDE 8
#define PRIMITIVE_VERTEX_STRIDE 8
#define PRIMITIVE_CUBE_VERTEX_COUNT 36
// Octahedron: 8 faces * 3 verts/face = 24 verts
#define PRIMITIVE_SPHERE_VERTEX_COUNT 336
// 4-sided cylinder: 4 side quads (2 tris each) + 4 top-cap + 4 bottom-cap tris
// = (8 + 4 + 4) tris * 3 = 48 verts
#define PRIMITIVE_CYLINDER_VERTEX_COUNT 144
