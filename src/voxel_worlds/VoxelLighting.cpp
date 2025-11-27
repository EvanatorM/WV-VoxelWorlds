#include <wv/voxel_worlds/VoxelLighting.h>

#include <wv/voxel_worlds/ChunkManager.h>
#include <wv/voxel_worlds/ChunkData.h>

namespace WillowVox::VoxelLighting
{
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

    void CalculateFullLighting(ChunkManager* chunkManager, ChunkData* chunkData)
    {
        chunkData->ClearLight();
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

    std::unordered_set<glm::ivec3> RemoveLightEmitter(ChunkManager* chunkManager, ChunkData* chunkData, int x, int y, int z)
    {
        return {};
    }
}