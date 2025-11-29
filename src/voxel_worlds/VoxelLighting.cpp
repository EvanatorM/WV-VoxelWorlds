#include <wv/voxel_worlds/VoxelLighting.h>

#include <wv/voxel_worlds/ChunkManager.h>
#include <wv/voxel_worlds/ChunkData.h>
#include <wv/core.h>

namespace WillowVox::VoxelLighting
{
    std::mutex lightingMutex;

    struct LightNode
    {
        LightNode(int x, int y, int z, ChunkData* chunk)
            : x(x), y(y), z(z), chunk(chunk)
        {}

        int x;
        int y;
        int z;
        ChunkData* chunk;
    };

    std::unordered_set<glm::ivec3> CalculateFullLighting(ChunkManager* chunkManager, ChunkData* chunkData)
    {
        chunkData->ClearLight();

        auto chunksToRemesh = CalculateSkyLighting(chunkManager, chunkData);
        chunksToRemesh.insert(chunkData->id);
        return chunksToRemesh;
    }

    std::unordered_set<glm::ivec3> CalculateSkyLighting(ChunkManager* chunkManager, ChunkData* chunkData)
    {
        std::unordered_set<glm::ivec3> chunksToRemesh;
        chunksToRemesh.insert(chunkData->id);
        std::queue<LightNode> sunlightQueue;

        for (int x = 0; x < CHUNK_SIZE; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE; ++z)
            {
                // Check if top block is empty
                if (chunkData->Get(x, CHUNK_SIZE - 1, z) == 0)
                {
                    // Set sky light level to maximum and enqueue
                    chunkData->SetSkyLightLevel(x, CHUNK_SIZE - 1, z, 15);
                    sunlightQueue.emplace(x, CHUNK_SIZE - 1, z, chunkData);
                }
            }
        }

        // Propagate sunlight
        while (!sunlightQueue.empty())
        {
            LightNode& node = sunlightQueue.front();
            int x = node.x;
            int y = node.y;
            int z = node.z;
            ChunkData* currentChunk = node.chunk;
            sunlightQueue.pop();

            int skyLightLevel = currentChunk->GetSkyLightLevel(x, y, z);


            // Propagate
            // Negative Y
            {
                int ny = y - 1;
                int nx = x;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (ny < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, -1, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, -1, 0));
                    ny += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated
                        if (targetChunk->GetSkyLightLevel(nx, ny, nz) + 1 <= skyLightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetSkyLightLevel(nx, ny, nz, skyLightLevel);
                            sunlightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Negative X
            {
                int nx = x - 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (nx < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(-1, 0, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(-1, 0, 0));
                    nx += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated
                        if (targetChunk->GetSkyLightLevel(nx, ny, nz) + 1 <= skyLightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetSkyLightLevel(nx, ny, nz, skyLightLevel - 1);
                            sunlightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Positive X
            {
                int nx = x + 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (nx >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(1, 0, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(1, 0, 0));
                    nx -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated
                        if (targetChunk->GetSkyLightLevel(nx, ny, nz) + 1 <= skyLightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetSkyLightLevel(nx, ny, nz, skyLightLevel - 1);
                            sunlightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Negative Z
            {
                int nx = x;
                int ny = y;
                int nz = z - 1;
                ChunkData* targetChunk = currentChunk;
                if (nz < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 0, -1)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 0, -1));
                    nz += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated
                        if (targetChunk->GetSkyLightLevel(nx, ny, nz) + 1 <= skyLightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetSkyLightLevel(nx, ny, nz, skyLightLevel - 1);
                            sunlightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Positive Z
            {
                int nx = x;
                int ny = y;
                int nz = z + 1;
                ChunkData* targetChunk = currentChunk;
                if (nz >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 0, 1)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 0, 1));
                    nz -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated
                        if (targetChunk->GetSkyLightLevel(nx, ny, nz) + 1 <= skyLightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetSkyLightLevel(nx, ny, nz, skyLightLevel - 1);
                            sunlightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
        }

        return chunksToRemesh;
    }

    std::unordered_set<glm::ivec3> AddLightEmitter(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z, int lightLevel)
    {
        // Initialize remesh vector
        std::unordered_set<glm::ivec3> chunksToRemesh;
        chunksToRemesh.insert(chunkData->id);

        // Initialize BFS
        std::queue<LightNode> lightQueue;

        // Set initial light level and enqueue
        chunkData->SetLightLevel(x, y, z, lightLevel);
        lightQueue.emplace(x, y, z, chunkData);

        // BFS for light propagation
        while (!lightQueue.empty())
        {
            // Get node from queue
            LightNode& node = lightQueue.front();
            int x = node.x;
            int y = node.y;
            int z = node.z;
            ChunkData* currentChunk = node.chunk;
            lightQueue.pop();

            // Get light level
            int lightLevel = currentChunk->GetLightLevel(x, y, z);

            // Propagate to neighbors
            // Negative X
            {
                int nx = x - 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (nx < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(-1, 0, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(-1, 0, 0));
                    nx += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated (only necessary if light level is 2 or more levels less than current node)
                        if (targetChunk->GetLightLevel(nx, ny, nz) + 2 <= lightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetLightLevel(nx, ny, nz, lightLevel - 1);
                            lightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }

            // ...

            // Positive X
            {
                int nx = x + 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (nx >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(1, 0, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(1, 0, 0));
                    nx -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated (only necessary if light level is 2 or more levels less than current node)
                        if (targetChunk->GetLightLevel(nx, ny, nz) + 2 <= lightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetLightLevel(nx, ny, nz, lightLevel - 1);
                            lightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Negative Z
            {
                int nx = x;
                int ny = y;
                int nz = z - 1;
                ChunkData* targetChunk = currentChunk;
                if (nz < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 0, -1)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 0, -1));
                    nz += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated (only necessary if light level is 2 or more levels less than current node)
                        if (targetChunk->GetLightLevel(nx, ny, nz) + 2 <= lightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetLightLevel(nx, ny, nz, lightLevel - 1);
                            lightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Positive Z
            {
                int nx = x;
                int ny = y;
                int nz = z + 1;
                ChunkData* targetChunk = currentChunk;
                if (nz >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 0, 1)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 0, 1));
                    nz -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated (only necessary if light level is 2 or more levels less than current node)
                        if (targetChunk->GetLightLevel(nx, ny, nz) + 2 <= lightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetLightLevel(nx, ny, nz, lightLevel - 1);
                            lightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Negative Y
            {
                int nx = x;
                int ny = y - 1;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (ny < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, -1, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, -1, 0));
                    ny += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated (only necessary if light level is 2 or more levels less than current node)
                        if (targetChunk->GetLightLevel(nx, ny, nz) + 2 <= lightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetLightLevel(nx, ny, nz, lightLevel - 1);
                            lightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
            // Positive Y
            {
                int nx = x;
                int ny = y + 1;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (ny >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 1, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 1, 0));
                    ny -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    // Only propagate through non-solid blocks
                    if (targetChunk->Get(nx, ny, nz) == 0)
                    {
                        // Check if light needs to be propagated (only necessary if light level is 2 or more levels less than current node)
                        if (targetChunk->GetLightLevel(nx, ny, nz) + 2 <= lightLevel)
                        {
                            // Set light level and enqueue
                            targetChunk->SetLightLevel(nx, ny, nz, lightLevel - 1);
                            lightQueue.emplace(nx, ny, nz, targetChunk);
                        }
                    }
                }
            }
        }

        return chunksToRemesh;
    }

    struct LightRemovalNode
    {
        LightRemovalNode(int x, int y, int z, ChunkData* chunk, int value)
            : x(x), y(y), z(z), chunk(chunk), value(value)
        {}
        int x;
        int y;
        int z;
        int value;
        ChunkData* chunk;
    };

    std::unordered_set<glm::ivec3> RemoveLightEmitter(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z)
    {
        // Initialize remesh vector
        std::unordered_set<glm::ivec3> chunksToRemesh;
        chunksToRemesh.insert(chunkData->id);

        // Initialize BFS
        std::queue<LightRemovalNode> lightRemovalQueue;
        std::queue<LightNode> lightPropagationQueue;

        // Set initial light level and enqueue
        int lightLevel = chunkData->GetLightLevel(x, y, z);
        lightRemovalQueue.emplace(x, y, z, chunkData, lightLevel);

        chunkData->SetLightLevel(x, y, z, 0);

        // BFS for light removal
        while (!lightRemovalQueue.empty())
        {
            // Get node from queue
            LightRemovalNode& node = lightRemovalQueue.front();
            int x = node.x;
            int y = node.y;
            int z = node.z;
            int lightLevel = node.value;
            ChunkData* currentChunk = node.chunk;
            lightRemovalQueue.pop();

            // Propagate to neighbors
            //      If their light level is not 0 and is less than the current node,
            //      add them to the queue and set their light level to zero.
            //      Else if it is >= current node, add it to light propagation queue.
            
            // Negative X
            {
                int nx = x - 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (nx < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(-1, 0, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(-1, 0, 0));
                    nx += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel != 0 && neighborLightLevel < lightLevel)
                    {
                        targetChunk->SetLightLevel(nx, ny, nz, 0);
                        lightRemovalQueue.emplace(nx, ny, nz, targetChunk, neighborLightLevel);
                    }
                    else if (neighborLightLevel >= lightLevel)
                    {
                        // Re-add light emitter
                        lightPropagationQueue.emplace(nx, ny, nz, targetChunk);
                    }
                }
            }
            // Positive X
            {
                int nx = x + 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (nx >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(1, 0, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(1, 0, 0));
                    nx -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel != 0 && neighborLightLevel < lightLevel)
                    {
                        targetChunk->SetLightLevel(nx, ny, nz, 0);
                        lightRemovalQueue.emplace(nx, ny, nz, targetChunk, neighborLightLevel);
                    }
                    else if (neighborLightLevel >= lightLevel)
                    {
                        // Re-add light emitter
                        lightPropagationQueue.emplace(nx, ny, nz, targetChunk);
                    }
                }
            }
            // Negative Y
            {
                int nx = x;
                int ny = y - 1;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (ny < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, -1, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, -1, 0));
                    ny += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel != 0 && neighborLightLevel < lightLevel)
                    {
                        targetChunk->SetLightLevel(nx, ny, nz, 0);
                        lightRemovalQueue.emplace(nx, ny, nz, targetChunk, neighborLightLevel);
                    }
                    else if (neighborLightLevel >= lightLevel)
                    {
                        // Re-add light emitter
                        lightPropagationQueue.emplace(nx, ny, nz, targetChunk);
                    }
                }
            }
            // Positive Y
            {
                int nx = x;
                int ny = y + 1;
                int nz = z;
                ChunkData* targetChunk = currentChunk;
                if (ny >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 1, 0)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 1, 0));
                    ny -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel != 0 && neighborLightLevel < lightLevel)
                    {
                        targetChunk->SetLightLevel(nx, ny, nz, 0);
                        lightRemovalQueue.emplace(nx, ny, nz, targetChunk, neighborLightLevel);
                    }
                    else if (neighborLightLevel >= lightLevel)
                    {
                        // Re-add light emitter
                        lightPropagationQueue.emplace(nx, ny, nz, targetChunk);
                    }
                }
            }
            // Negative Z
            {
                int nx = x;
                int ny = y;
                int nz = z - 1;
                ChunkData* targetChunk = currentChunk;
                if (nz < 0)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 0, -1)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 0, -1));
                    nz += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel != 0 && neighborLightLevel < lightLevel)
                    {
                        targetChunk->SetLightLevel(nx, ny, nz, 0);
                        lightRemovalQueue.emplace(nx, ny, nz, targetChunk, neighborLightLevel);
                    }
                    else if (neighborLightLevel >= lightLevel)
                    {
                        // Re-add light emitter
                        lightPropagationQueue.emplace(nx, ny, nz, targetChunk);
                    }
                }
            }
            // Positive Z
            {
                int nx = x;
                int ny = y;
                int nz = z + 1;
                ChunkData* targetChunk = currentChunk;
                if (nz >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(currentChunk->id + glm::ivec3(0, 0, 1)).get();
                    chunksToRemesh.insert(currentChunk->id + glm::ivec3(0, 0, 1));
                    nz -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel != 0 && neighborLightLevel < lightLevel)
                    {
                        targetChunk->SetLightLevel(nx, ny, nz, 0);
                        lightRemovalQueue.emplace(nx, ny, nz, targetChunk, neighborLightLevel);
                    }
                    else if (neighborLightLevel >= lightLevel)
                    {
                        // Re-add light emitter
                        lightPropagationQueue.emplace(nx, ny, nz, targetChunk);
                    }
                }
            }
        }

        // Re-add light from neighbors
        while (!lightPropagationQueue.empty())
        {
            LightNode& node = lightPropagationQueue.front();
            int x = node.x;
            int y = node.y;
            int z = node.z;
            ChunkData* currentChunk = node.chunk;
            lightPropagationQueue.pop();

            int lightLevel = currentChunk->GetLightLevel(x, y, z);
            auto newChunks = AddLightEmitter(chunkManager, currentChunk, x, y, z, lightLevel);
            chunksToRemesh.insert(newChunks.begin(), newChunks.end());
        }

        return chunksToRemesh;
    }

    std::unordered_set<glm::ivec3> AddLightBlocker(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z)
    {
        // Get light level
        int lightLevel = chunkData->GetLightLevel(x, y, z);

        // If light level > 0, remove light that is now being blocked by the new blocker
        if (lightLevel > 0)
        {
            return RemoveLightEmitter(chunkManager, chunkData, x, y, z);
        }
        
        return {};
    }
    
    std::unordered_set<glm::ivec3> RemoveLightBlocker(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z)
    {
        // Get surrounding light levels and find the max
        int maxLightLevel = 0;
        ChunkData* maxLightChunk = chunkData;
        int maxLightX, maxLightY, maxLightZ;

        {
            // Negative X
            {
                int nx = x - 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = chunkData;
                if (nx < 0)
                {
                    targetChunk = chunkManager->GetChunkData(chunkData->id + glm::ivec3(-1, 0, 0)).get();
                    nx += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel > maxLightLevel)
                    {
                        maxLightLevel = neighborLightLevel;
                        maxLightChunk = targetChunk;
                        maxLightX = nx;
                        maxLightY = ny;
                        maxLightZ = nz;
                    }
                }
            }
            // Positive X
            {
                int nx = x + 1;
                int ny = y;
                int nz = z;
                ChunkData* targetChunk = chunkData;
                if (nx >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(chunkData->id + glm::ivec3(1, 0, 0)).get();
                    nx -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel > maxLightLevel)
                    {
                        maxLightLevel = neighborLightLevel;
                        maxLightChunk = targetChunk;
                        maxLightX = nx;
                        maxLightY = ny;
                        maxLightZ = nz;
                    }
                }
            }
            // Negative Y
            {
                int nx = x;
                int ny = y - 1;
                int nz = z;
                ChunkData* targetChunk = chunkData;
                if (ny < 0)
                {
                    targetChunk = chunkManager->GetChunkData(chunkData->id + glm::ivec3(0, -1, 0)).get();
                    ny += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel > maxLightLevel)
                    {
                        maxLightLevel = neighborLightLevel;
                        maxLightChunk = targetChunk;
                        maxLightX = nx;
                        maxLightY = ny;
                        maxLightZ = nz;
                    }
                }
            }
            // Positive Y
            {
                int nx = x;
                int ny = y + 1;
                int nz = z;
                ChunkData* targetChunk = chunkData;
                if (ny >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(chunkData->id + glm::ivec3(0, 1, 0)).get();
                    ny -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel > maxLightLevel)
                    {
                        maxLightLevel = neighborLightLevel;
                        maxLightChunk = targetChunk;
                        maxLightX = nx;
                        maxLightY = ny;
                        maxLightZ = nz;
                    }
                }
            }
            // Negative Z
            {
                int nx = x;
                int ny = y;
                int nz = z - 1;
                ChunkData* targetChunk = chunkData;
                if (nz < 0)
                {
                    targetChunk = chunkManager->GetChunkData(chunkData->id + glm::ivec3(0, 0, -1)).get();
                    nz += CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel > maxLightLevel)
                    {
                        maxLightLevel = neighborLightLevel;
                        maxLightChunk = targetChunk;
                        maxLightX = nx;
                        maxLightY = ny;
                        maxLightZ = nz;
                    }
                }
            }
            // Positive Z
            {
                int nx = x;
                int ny = y;
                int nz = z + 1;
                ChunkData* targetChunk = chunkData;
                if (nz >= CHUNK_SIZE)
                {
                    targetChunk = chunkManager->GetChunkData(chunkData->id + glm::ivec3(0, 0, 1)).get();
                    nz -= CHUNK_SIZE;
                }
                if (targetChunk)
                {
                    int neighborLightLevel = targetChunk->GetLightLevel(nx, ny, nz);
                    if (neighborLightLevel > maxLightLevel)
                    {
                        maxLightLevel = neighborLightLevel;
                        maxLightChunk = targetChunk;
                        maxLightX = nx;
                        maxLightY = ny;
                        maxLightZ = nz;
                    }
                }
            }
        }

        if (maxLightLevel == 0)
        {
            // No surrounding light, nothing to add
            return {};
        }

        // Add light from the strongest neighbor
        return AddLightEmitter(chunkManager, maxLightChunk, maxLightX, maxLightY, maxLightZ, maxLightLevel);
    }
}