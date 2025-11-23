#pragma once

#include <wv/voxel_worlds/ChunkManager.h>
#include <wv/core.h>

namespace WillowVox
{
    struct VoxelRaycastResult
    {
        bool hit;
        float hitX;
        float hitY;
        float hitZ;
    };

    VoxelRaycastResult VoxelRaycast(ChunkManager& chunkManager, const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
}