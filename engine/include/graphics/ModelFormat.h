#pragma once
#include <cstddef>
#include <cstdint>

#include "Types.h"

// ---------------------------------------------------------------------------
// Baked model format (.bkm) produced by pack_assets.py.
//
// Raylib's runtime OBJ/glTF loader is gone; models are baked offline into
// separated, UNINDEXED triangle arrays — exactly the layout ps2gl needs
// (stride-0 vertex/normal/uv pointers) and the GIFTAG builder consumes directly.
//
// On-disk layout (little-endian, all sections 16-byte aligned):
//   BakedModelHeader
//   BakedMeshEntry   [meshCount]
//   BakedMaterialEntry [materialCount]
//   <geometry payload>   (referenced by absolute byte offset from file start)
// ---------------------------------------------------------------------------

#define BAKED_MODEL_MAGIC 0x324D4B42u // "BKM2" little-endian
#define BAKED_MODEL_VERSION 1u

struct BakedModelHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t meshCount;
    uint32_t materialCount;
};

struct BakedMeshEntry
{
    uint32_t vertexCount; // triangleCount * 3 (unindexed)
    uint32_t materialIndex; // index into the material table (0 if none)
    uint32_t vertsOffset; // byte offset to 3*float*vertexCount (required)
    uint32_t normsOffset; // byte offset to 3*float*vertexCount (0 = none)
    uint32_t uvsOffset; // byte offset to 2*float*vertexCount (0 = none)
    uint32_t reserved;
};

struct BakedMaterialEntry
{
    // Reference to the diffuse texture: an index into the owning .ps2a's
    // dependency list, resolved to a Texture2D at load time. 0xFFFFFFFF = none.
    uint32_t diffuseTexRef;
    uint32_t reserved;
};

#define BAKED_MODEL_TEXREF_NONE 0xFFFFFFFFu

// Resolve a material's diffuse texture reference (an index into the owning
// .ps2a's dependency list) to a resource handle. Returns the handle (>= 0) or
// -1 when unavailable. The handle is stored in the material and resolved to a
// live Texture2D at draw time (texture deps stream in asynchronously).
typedef int32_t (*ModelTextureResolver)(uint32_t diffuseTexRef, void* user);

// Parse a baked-model blob into `outModel`. Geometry arrays are copied into
// fresh 16-byte-aligned heap allocations (so `data` may be freed afterwards).
// `resolver` may be null (all materials become untextured). Returns true on
// success; call Model_FreeBaked to release everything it allocated.
bool Model_LoadBaked(const void* data, size_t size, Model* outModel,
                     ModelTextureResolver resolver, void* resolverUser);

// Free all heap allocations made by Model_LoadBaked and zero the struct.
// Does NOT release textures (those are owned by the resource manager).
void Model_FreeBaked(Model* model);
