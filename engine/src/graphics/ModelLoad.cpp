#include "../include/graphics/ModelFormat.h"

#include <cstring>
#include <malloc.h>

#include "EngineDebug.h"

namespace
{
float* AllocFloats(size_t count)
{
    // 16-byte aligned so the arrays are safe for qword DMA / cache ops.
    return static_cast<float*>(memalign(16, count * sizeof(float)));
}
} // namespace

bool Model_LoadBaked(const void* data, size_t size, Model* outModel,
                     ModelTextureResolver resolver, void* resolverUser)
{
    if (!data || !outModel || size < sizeof(BakedModelHeader))
    {
        Engine_LogError("Model: null/short blob (%zu bytes).", size);
        return false;
    }

    std::memset(outModel, 0, sizeof(Model));

    const uint8_t* base = static_cast<const uint8_t*>(data);
    BakedModelHeader hdr;
    std::memcpy(&hdr, base, sizeof(hdr));

    if (hdr.magic != BAKED_MODEL_MAGIC)
    {
        Engine_LogError("Model: bad magic.");
        return false;
    }
    if (hdr.version != BAKED_MODEL_VERSION)
    {
        Engine_LogError("Model: unsupported version %u.", hdr.version);
        return false;
    }
    if (hdr.meshCount == 0 || hdr.meshCount > 65535u || hdr.materialCount > 65535u)
    {
        Engine_LogError("Model: bad counts (meshes=%u, materials=%u).", hdr.meshCount, hdr.materialCount);
        return false;
    }

    // Header + mesh table + material table must all fit inside the blob.
    const size_t meshTableOff = sizeof(BakedModelHeader);
    const size_t matTableOff = meshTableOff + static_cast<size_t>(hdr.meshCount) * sizeof(BakedMeshEntry);
    const size_t payloadOff = matTableOff + static_cast<size_t>(hdr.materialCount) * sizeof(BakedMaterialEntry);
    if (payloadOff > size)
    {
        Engine_LogError("Model: truncated tables.");
        return false;
    }

    outModel->meshCount = static_cast<int>(hdr.meshCount);
    outModel->meshes = static_cast<Mesh*>(calloc(hdr.meshCount, sizeof(Mesh)));
    outModel->meshMaterial = static_cast<int*>(calloc(hdr.meshCount, sizeof(int)));
    if (!outModel->meshes || !outModel->meshMaterial)
    {
        Engine_LogError("Model: OOM (mesh tables).");
        Model_FreeBaked(outModel);
        return false;
    }

    // --- Materials (resolve diffuse textures via the caller's resolver) ---
    if (hdr.materialCount > 0)
    {
        outModel->materialCount = static_cast<int>(hdr.materialCount);
        outModel->materials = static_cast<Material*>(calloc(hdr.materialCount, sizeof(Material)));
        if (!outModel->materials)
        {
            Engine_LogError("Model: OOM (materials).");
            Model_FreeBaked(outModel);
            return false;
        }

        for (uint32_t m = 0; m < hdr.materialCount; ++m)
        {
            BakedMaterialEntry me;
            std::memcpy(&me, base + matTableOff + m * sizeof(BakedMaterialEntry), sizeof(me));

            MaterialMap& map = outModel->materials[m].maps[MATERIAL_MAP_DIFFUSE];
            std::memset(&map.texture, 0, sizeof(map.texture));
            map.textureResourceId = -1;
            if (resolver && me.diffuseTexRef != BAKED_MODEL_TEXREF_NONE)
                map.textureResourceId = resolver(me.diffuseTexRef, resolverUser);
        }
    }

    // --- Meshes (copy separated arrays into aligned heap allocations) ---
    for (uint32_t i = 0; i < hdr.meshCount; ++i)
    {
        BakedMeshEntry me;
        std::memcpy(&me, base + meshTableOff + i * sizeof(BakedMeshEntry), sizeof(me));

        Mesh& mesh = outModel->meshes[i];
        mesh.vertexCount = static_cast<int>(me.vertexCount);
        mesh.indices = nullptr;

        if (me.vertexCount == 0 || me.vertsOffset == 0)
        {
            Engine_LogError("Model: mesh %u has no vertices.", i);
            Model_FreeBaked(outModel);
            return false;
        }

        const size_t vBytes = static_cast<size_t>(me.vertexCount) * 3 * sizeof(float);
        if (static_cast<size_t>(me.vertsOffset) + vBytes > size)
        {
            Engine_LogError("Model: mesh %u vertices out of bounds.", i);
            Model_FreeBaked(outModel);
            return false;
        }
        mesh.vertices = AllocFloats(static_cast<size_t>(me.vertexCount) * 3);
        if (!mesh.vertices)
        {
            Model_FreeBaked(outModel);
            return false;
        }
        std::memcpy(mesh.vertices, base + me.vertsOffset, vBytes);

        if (me.normsOffset != 0)
        {
            const size_t nBytes = static_cast<size_t>(me.vertexCount) * 3 * sizeof(float);
            if (static_cast<size_t>(me.normsOffset) + nBytes > size)
            {
                Engine_LogError("Model: mesh %u normals out of bounds.", i);
                Model_FreeBaked(outModel);
                return false;
            }
            mesh.normals = AllocFloats(static_cast<size_t>(me.vertexCount) * 3);
            if (!mesh.normals)
            {
                Model_FreeBaked(outModel);
                return false;
            }
            std::memcpy(mesh.normals, base + me.normsOffset, nBytes);
        }

        if (me.uvsOffset != 0)
        {
            const size_t uBytes = static_cast<size_t>(me.vertexCount) * 2 * sizeof(float);
            if (static_cast<size_t>(me.uvsOffset) + uBytes > size)
            {
                Engine_LogError("Model: mesh %u texcoords out of bounds.", i);
                Model_FreeBaked(outModel);
                return false;
            }
            mesh.texcoords = AllocFloats(static_cast<size_t>(me.vertexCount) * 2);
            if (!mesh.texcoords)
            {
                Model_FreeBaked(outModel);
                return false;
            }
            std::memcpy(mesh.texcoords, base + me.uvsOffset, uBytes);
        }

        uint32_t matIdx = me.materialIndex;
        if (hdr.materialCount == 0 || matIdx >= hdr.materialCount)
            matIdx = 0;
        outModel->meshMaterial[i] = static_cast<int>(matIdx);
    }

    Engine_LogInfo("Model: baked load OK (%d meshes, %d materials).", outModel->meshCount, outModel->materialCount);
    return true;
}

void Model_FreeBaked(Model* model)
{
    if (!model)
        return;

    if (model->meshes)
    {
        for (int i = 0; i < model->meshCount; ++i)
        {
            free(model->meshes[i].vertices);
            free(model->meshes[i].normals);
            free(model->meshes[i].texcoords);
        }
        free(model->meshes);
    }
    free(model->materials);
    free(model->meshMaterial);
    std::memset(model, 0, sizeof(Model));
}
