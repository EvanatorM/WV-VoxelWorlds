#pragma once

#include <wv/core.h>
#include <wv/voxel_worlds/BlockRegistry.h>
#include <wv/voxel_worlds/ChunkDefines.h>
#include <cassert>
#include <algorithm>

namespace WillowVox
{
    enum class LightingStage : uint8_t
    {
        WorldGenInProgress = 0,
        ReadyForLighting = 1,
        LocalLightCalculated = 2
    };

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

        constexpr bool IsEmpty() noexcept
        {
            for (int i = 0; i < CHUNK_VOLUME; ++i)
            {
                if (voxels[i] != 0)
                    return false;
            }
            return true;
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
            for (auto& v : skyLightLevels)
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

        inline int GetSkyLightLevel(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return skyLightLevels[Index(x, y, z)];
        }

        inline void SetSkyLightLevel(int x, int y, int z, int value) noexcept
        {
            assert(InBounds(x, y, z));
            skyLightLevels[Index(x, y, z)] = value;
        }

        // Block data
        BlockId voxels[CHUNK_VOLUME];

        // Light data
        int lightLevels[CHUNK_VOLUME];
        int skyLightLevels[CHUNK_VOLUME];

        // To be used by client-implemented world generation functions
        uint8_t worldGenStage = 0;
        // To be used by the lighting engine
        LightingStage lightingStage = LightingStage::WorldGenInProgress;

        glm::ivec3 id;
    };
}