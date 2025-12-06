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

        inline void Clear() noexcept
        {
            for (auto& v : voxels)
                v = 0;
            for (auto& v : lightData)
                v = 0;
        }

        inline int GetSkyLight(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return (lightData[Index(x, y, z)] >> 12) & 0x0F;
        }

        inline void SetSkyLight(int x, int y, int z, int value) noexcept
        {
            assert(InBounds(x, y, z));
            int index = Index(x, y, z);
            lightData[index] = (lightData[index] & 0x0FFF) | (value << 12);
        }

        inline int GetRedLight(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return (lightData[Index(x, y, z)] >> 8) & 0x0F;
        }

        inline void SetRedLight(int x, int y, int z, int value) noexcept
        {
            assert(InBounds(x, y, z));
            int index = Index(x, y, z);
            lightData[index] = (lightData[index] & 0xF0FF) | (value << 8);
        }

        inline int GetGreenLight(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return (lightData[Index(x, y, z)] >> 4) & 0x0F;
        }

        inline void SetGreenLight(int x, int y, int z, int value) noexcept
        {
            assert(InBounds(x, y, z));
            int index = Index(x, y, z);
            lightData[index] = (lightData[index] & 0xFF0F) | (value << 4);
        }

        inline int GetBlueLight(int x, int y, int z) const noexcept
        {
            assert(InBounds(x, y, z));
            return lightData[Index(x, y, z)] & 0x0F;
        }

        inline void SetBlueLight(int x, int y, int z, int value) noexcept
        {
            assert(InBounds(x, y, z));
            int index = Index(x, y, z);
            lightData[index] = (lightData[index] & 0xFFF0) | (value);
        }

        inline uint16_t GetLightData(int x, int y, int z) noexcept
        {
            assert(InBounds(x, y, z));
            return lightData[Index(x, y, z)];
        }

        // Block data
        BlockId voxels[CHUNK_VOLUME];
        // Light data (SSSS RRRR GGGG BBBB)
        uint16_t lightData[CHUNK_VOLUME];

        // To be used by client-implemented world generation functions
        uint8_t worldGenStage = 0;
        // To be used by the lighting engine
        LightingStage lightingStage = LightingStage::WorldGenInProgress;

        glm::ivec3 id;
    };
}