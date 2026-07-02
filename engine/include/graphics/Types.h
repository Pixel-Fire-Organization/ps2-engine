#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Engine-native graphics types.
//
// These replace the type definitions the engine previously pulled in from
// <raylib.h>. Field names and layouts intentionally mirror raylib's so that
// existing consumers (scripting camera bindings, the resource manager, the
// renderers, input) compile unchanged; only code that called raylib *functions*
// was rewritten. Fonts / sound types are intentionally NOT provided — those
// paths were dropped when raylib was removed.
// ---------------------------------------------------------------------------

// --- Math ---------------------------------------------------------------

// Vector2, 2 components
typedef struct Vector2
{
    float x;
    float y;
} Vector2;

// Vector3, 3 components
typedef struct Vector3
{
    float x;
    float y;
    float z;
} Vector3;

// Vector4, 4 components
typedef struct Vector4
{
    float x;
    float y;
    float z;
    float w;
} Vector4;

// Quaternion, 4 components (Vector4 alias)
typedef Vector4 Quaternion;

// Matrix, 4x4 components, column major, OpenGL style, right-handed
typedef struct Matrix
{
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;

// Color, 4 components, R8G8B8A8 (32bit)
typedef struct Color
{
    unsigned char r, g, b, a;
} Color;

// --- Texture pixel formats ---------------------------------------------

// GS-uploadable pixel formats. TIM2 assets are baked to one of these; the
// renderer's UploadTexture consumes the raw pixels plus this format.
enum class PixelFormat : uint8_t
{
    RGBA32, // 32-bit R8G8B8A8 — GS PSMCT32
    RGBA16, // 16-bit R5G5B5A1 — GS PSMCT16
};

// --- Camera ------------------------------------------------------------

#define CAMERA_PERSPECTIVE 0
#define CAMERA_ORTHOGRAPHIC 1

// Camera3D, defines a 3D camera used to render the scene.
typedef struct Camera3D
{
    Vector3 position; // camera position
    Vector3 target; // camera look-at target
    Vector3 up; // camera up vector (rotation over its axis)
    float fovy; // field-of-view aperture in Y (degrees)
    int projection; // CAMERA_PERSPECTIVE or CAMERA_ORTHOGRAPHIC
} Camera3D;

// Camera2D, defines a 2D camera used to render the UI / HUD.
typedef struct Camera2D
{
    Vector2 offset; // camera offset (displacement from target)
    Vector2 target; // camera target (rotation/zoom origin)
    float rotation; // camera rotation in degrees
    float zoom; // camera zoom (scaling), 1.0f by default
} Camera2D;

// --- Texture / Image ----------------------------------------------------

// Texture2D, a texture resident in (or bound to) GS VRAM.
// `id` is a backend-defined handle: the ps2gl GL texture name for the PS2GL
// renderer, or a GS-VRAM token for the GIFTAG renderer. `id == 0` means "not
// uploaded / invalid" in both backends.
typedef struct Texture2D
{
    uint32_t id; // backend handle (ps2gl GL name or GS-VRAM token); 0 = invalid
    int width; // texel width
    int height; // texel height
    int format; // PixelFormat value the texture was uploaded with
} Texture2D;

// Image, a decoded texture living in main RAM. Produced by the TIM2 parser and
// consumed by the renderer's UploadTexture; short-lived (freed after upload).
typedef struct Image
{
    void* data; // 16-byte-aligned pixel data in main RAM
    int width;
    int height;
    int format; // PixelFormat value
} Image;

// --- Mesh / Material / Model -------------------------------------------

// Mesh, separated (stride-0) unindexed triangle arrays as required by ps2gl and
// used directly by the GIFTAG packet builder. `indices` is always null for
// baked models but retained so existing "indices != nullptr → unsupported"
// guards remain valid.
typedef struct Mesh
{
    int vertexCount; // number of vertices (triangleCount * 3)
    float* vertices; // 3 floats per vertex
    float* normals; // 3 floats per vertex (may be null)
    float* texcoords; // 2 floats per vertex (may be null)
    unsigned short* indices; // always null for baked models
} Mesh;

#define MATERIAL_MAP_DIFFUSE 0
#define MAX_MATERIAL_MAPS 1

// A single material texture map slot. `textureResourceId` is the resource
// handle of the diffuse texture (-1 if none); because a model's texture
// dependencies stream in asynchronously, renderers resolve it to a live
// Texture2D at draw time rather than caching it here.
typedef struct MaterialMap
{
    Texture2D texture; // optional cached texture (may be zeroed; resolve via id)
    int32_t textureResourceId;
} MaterialMap;

// Material, only the diffuse map is used on the PS2 fixed-function pipeline.
typedef struct Material
{
    MaterialMap maps[MAX_MATERIAL_MAPS];
} Material;

// Model, a collection of unindexed meshes plus their materials.
typedef struct Model
{
    int meshCount; // number of meshes
    int materialCount; // number of materials
    Mesh* meshes; // meshes array
    Material* materials; // materials array
    int* meshMaterial; // material index per mesh (may be null → material 0)
} Model;
