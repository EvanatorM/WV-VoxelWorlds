#pragma once

#include <wv/core.h>
#include <wv/voxel_worlds/BlockRegistry.h>
#include <wv/voxel_worlds/ChunkDefines.h>
#include <cassert>
#include <algorithm>

namespace WillowVox
{
    class ChunkManager;

    struct ChunkData
    {
        ChunkData(const glm::ivec3& id, ChunkManager* manager)
            : id(id), chunkManager(manager)
        {
            blockRegistry = &BlockRegistry::GetInstance();

            Clear();
        }

        static constexpr bool InBounds(int x, int y, int z) noexcept
        {
            return 0 <= x && x < CHUNK_SIZE &&
                0 <= y && y < CHUNK_SIZE &&
                0 <= z && z < CHUNK_SIZE;
        }

        static constexpr int Index(int x, int y, int z) noexcept
        {
            return y + CHUNK_SIZE * (x + CHUNK_SIZE * z);
        }

        inline BlockId Get(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return voxels[Index(x, y, z)];
        }

        inline void Set(int x, int y, int z, BlockId value) noexcept
        {
            assert(InBounds(x, y, z));

            int i = Index(x, y, z);
            BlockId oldValue = voxels[i];
            if (blockRegistry->GetBlock(oldValue).lightEmitter)
                lightEmitters.erase(std::remove(lightEmitters.begin(), lightEmitters.end(), glm::ivec3(x, y, z)), lightEmitters.end());

            voxels[Index(x, y, z)] = value;
            if (blockRegistry->GetBlock(value).lightEmitter)
                lightEmitters.push_back({ x, y, z});
        }

        inline void Clear() noexcept
        {
            for (auto& v : voxels)
                v = 0;
            for (auto& v : lightLevels)
                v = 0;
        }

        inline int GetLightLevel(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return lightLevels[Index(x, y, z)];
        }

        inline void SetLightLevel(int x, int y, int z, int value) noexcept
        {
            assert(InBounds(x, y, z));
            lightLevels[Index(x, y, z)] = value;
        }

        void CalculateLighting();

        BlockId voxels[CHUNK_VOLUME];
        int lightLevels[CHUNK_VOLUME];
        std::vector<glm::ivec3> lightEmitters;

        glm::ivec3 id;

    private:
        BlockRegistry* blockRegistry;

        ChunkManager* chunkManager;
        std::shared_ptr<ChunkData> surroundingData[26];
        bool surroundingDataCached = false;
    };
}