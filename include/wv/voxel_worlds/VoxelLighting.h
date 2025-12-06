#pragma once

#include <wv/wvpch.h>
#include <unordered_set>

namespace WillowVox
{
    class ChunkManager;
    class ChunkData;

    namespace VoxelLighting
    {
        // Calculate full lighting for the given chunk
        // Only do this during initial generation as it is expensive
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> CalculateFullLighting(ChunkManager* chunkManager, ChunkData* chunkData);

        // Add a skylight blocker at the given local chunk position
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> AddSkyLightBlocker(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z);

        // Remove a skylight blocker at the given local chunk position
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> RemoveSkyLightBlocker(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z);

        // Add a light emitter at the given local chunk position with the given light level
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> AddLightEmitter(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z, int redLight, int greenLight, int blueLight);

        // Remove a light emitter at the given local chunk position
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> RemoveLightEmitter(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z);

        // Add a light blocker at the given local chunk position
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> AddLightBlocker(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z);
        
        // Remove a light blocker at the given local chunk position
        // Returns a set of chunk ids that need to be remeshed
        std::unordered_set<glm::ivec3> RemoveLightBlocker(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z);

        extern std::mutex lightingMutex;
        extern std::mutex skyLightingMutex;
    }
}