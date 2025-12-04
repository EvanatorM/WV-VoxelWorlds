#pragma once

#include <wv/voxel_worlds/ChunkData.h>

namespace WillowVox
{
    class WorldGen
    {
    public:
        virtual void Generate(ChunkData* data, const glm::ivec3& chunkPos, uint8_t worldGenStage) = 0;
    };
}