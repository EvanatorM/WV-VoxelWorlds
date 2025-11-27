#pragma once

#include <wv/core.h>
#include <wv/voxel_worlds/BlockRegistry.h>
#include <wv/voxel_worlds/ChunkDefines.h>
#include <cassert>
#include <algorithm>

namespace WillowVox
{
    struct ChunkData
    {
        ChunkData(const glm::ivec3& id)
            : id(id)
        {
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
            voxels[Index(x, y, z)] = value;
        }

        inline void ClearLight() noexcept
        {
            for (auto& v : lightLevels)
                v = 0;
        }

        inline void ClearBlocks() noexcept
        {
            for (auto& v : voxels)
                v = 0;
        }

        inline void Clear() noexcept
        {
            ClearBlocks();
            ClearLight();
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

        BlockId voxels[CHUNK_VOLUME];
        int lightLevels[CHUNK_VOLUME];

        glm::ivec3 id;
    };
}