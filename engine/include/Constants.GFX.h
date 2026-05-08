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

#define PRIMITIVE_CUBE_VERTEX_STRIDE 8
#define PRIMITIVE_VERTEX_STRIDE 8
#define PRIMITIVE_CUBE_VERTEX_COUNT 36
// Octahedron: 8 faces * 3 verts/face = 24 verts
#define PRIMITIVE_SPHERE_VERTEX_COUNT 336
// 4-sided cylinder: 4 side quads (2 tris each) + 4 top-cap + 4 bottom-cap tris
// = (8 + 4 + 4) tris * 3 = 48 verts
#define PRIMITIVE_CYLINDER_VERTEX_COUNT 144
