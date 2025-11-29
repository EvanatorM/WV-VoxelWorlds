#include <wv/voxel_worlds/ChunkManager.h>
#include <wv/voxel_worlds/WorldGen.h>
#include <wv/voxel_worlds/VoxelLighting.h>
#include <algorithm>
#include <chrono>

namespace WillowVox
{
    ChunkManager::ChunkManager(WorldGen* worldGen, int numChunkThreads, int worldSizeX, int worldMinY, int worldMaxY, int worldSizeZ)
        : m_worldGen(worldGen), m_chunkThreadPool(512), m_worldSizeX(worldSizeX), m_worldMinY(worldMinY), m_worldMaxY(worldMaxY), m_worldSizeZ(worldSizeZ)
    {
        // Load assets
        auto& am = AssetManager::GetInstance();
        m_chunkShader = am.GetAsset<Shader>("chunk_shader");
        m_chunkTexture = am.GetAsset<Texture>("chunk_texture");

        // Start chunk thread pool
        m_chunkThreadPool.Start(numChunkThreads);

        // Start chunk thread
        m_chunkThread = std::thread(&ChunkManager::ChunkThread, this);
    }

    ChunkManager::~ChunkManager()
    {
        m_chunkThreadShouldStop = true;
        m_chunkThread.join();

        m_chunkThreadPool.Stop();
    }

    inline void StartChunkMeshJob(ThreadPool& pool, std::shared_ptr<ChunkRenderer> renderer, bool highPriority = false)
    {
        if (!renderer)
            return;

        std::weak_ptr<ChunkRenderer> weakChunkPtr = renderer;
        pool.QueueJob([weakChunkPtr] {
            if (auto chunkPtr = weakChunkPtr.lock())
            {
                uint32_t currentVersion = ++chunkPtr->m_version;
                std::lock_guard<std::mutex> lock(chunkPtr->m_generationMutex);
                chunkPtr->GenerateMesh(currentVersion);
            }
        }, highPriority);
    }

    inline void StartBatchChunkMeshJob(ThreadPool& pool, std::vector<std::shared_ptr<ChunkRenderer>> renderers, bool highPriority = false)
    {
        std::vector<std::weak_ptr<ChunkRenderer>> weakPtrs;
        std::vector<uint32_t> versions;
        for (auto& r : renderers)
        {
            if (!r)
                continue;
            
            std::weak_ptr<ChunkRenderer> weakChunkPtr = r;
            weakPtrs.push_back(weakChunkPtr);
            versions.push_back(++r->m_version);
        }

        pool.QueueJob([weakPtrs, versions, highPriority] {
            for (size_t i = 0; i < weakPtrs.size(); ++i)
            {
                auto& weakChunkPtr = weakPtrs[i];
                uint32_t currentVersion = versions[i];
                if (auto chunkPtr = weakChunkPtr.lock())
                {
                    std::lock_guard<std::mutex> lock(chunkPtr->m_generationMutex);
                    chunkPtr->GenerateMesh(currentVersion, highPriority);
                }
            }

            for (auto& weakChunkPtr : weakPtrs)
            {
                if (auto chunkPtr = weakChunkPtr.lock())
                    chunkPtr->MarkDirty();
            }
        }, highPriority);
    }

    inline void StartLightingRecalculationJob(ThreadPool& pool, ChunkManager* chunkManager, std::shared_ptr<ChunkData> chunkData, std::shared_ptr<ChunkRenderer> renderer, bool highPriority = false)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        std::weak_ptr<ChunkRenderer> weakChunkRendererPtr = renderer;
        pool.QueueJob([chunkManager, weakChunkDataPtr, weakChunkRendererPtr] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                {
                    WillowVox::VoxelLighting::CalculateFullLighting(chunkManager, chunkDataPtr.get());
                }
                if (auto chunkRendererPtr = weakChunkRendererPtr.lock())
                {
                    uint32_t currentVersion = ++chunkRendererPtr->m_version;
                    std::lock_guard<std::mutex> lock(chunkRendererPtr->m_generationMutex);
                    chunkRendererPtr->GenerateMesh(currentVersion);
                }
            }
        }, highPriority);
    }

    inline void StartLightAddJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, int lightLevel, bool highPriority = false)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.QueueJob([&chunkManager, weakChunkDataPtr, x, y, z, lightLevel] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::lightingMutex);
                auto chunksToRemesh = WillowVox::VoxelLighting::AddLightEmitter(&chunkManager, chunkDataPtr.get(), x, y, z, lightLevel);

                // Remesh affected chunks
                for (auto& chunkId : chunksToRemesh)
                {
                    auto renderer = chunkManager.GetChunkRenderer(chunkId);
                    if (renderer)
                    {
                        uint32_t currentVersion = ++renderer->m_version;
                        std::lock_guard<std::mutex> lock(renderer->m_generationMutex);
                        renderer->GenerateMesh(currentVersion);
                    }
                }
            }
        }, highPriority);
    }

    inline void StartLightRemovalJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, bool highPriority = false)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.QueueJob([&chunkManager, weakChunkDataPtr, x, y, z] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::lightingMutex);
                auto chunksToRemesh = WillowVox::VoxelLighting::RemoveLightEmitter(&chunkManager, chunkDataPtr.get(), x, y, z);

                // Remesh affected chunks
                for (auto& chunkId : chunksToRemesh)
                {
                    auto renderer = chunkManager.GetChunkRenderer(chunkId);
                    if (renderer)
                    {
                        uint32_t currentVersion = ++renderer->m_version;
                        std::lock_guard<std::mutex> lock(renderer->m_generationMutex);
                        renderer->GenerateMesh(currentVersion);
                    }
                }
            }
        }, highPriority);
    }

    inline void StartLightBlockerAddJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, bool highPriority = false)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.QueueJob([&chunkManager, weakChunkDataPtr, x, y, z] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::lightingMutex);
                auto chunksToRemesh = WillowVox::VoxelLighting::AddLightBlocker(&chunkManager, chunkDataPtr.get(), x, y, z);

                // Remesh affected chunks
                for (auto& chunkId : chunksToRemesh)
                {
                    auto renderer = chunkManager.GetChunkRenderer(chunkId);
                    if (renderer)
                    {
                        uint32_t currentVersion = ++renderer->m_version;
                        std::lock_guard<std::mutex> lock(renderer->m_generationMutex);
                        renderer->GenerateMesh(currentVersion);
                    }
                }
            }
        }, highPriority);
    }

    inline void StartLightBlockerRemovalJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, bool highPriority = false)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.QueueJob([&chunkManager, weakChunkDataPtr, x, y, z] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::lightingMutex);
                auto chunksToRemesh = WillowVox::VoxelLighting::RemoveLightBlocker(&chunkManager, chunkDataPtr.get(), x, y, z);

                // Remesh affected chunks
                for (auto& chunkId : chunksToRemesh)
                {
                    auto renderer = chunkManager.GetChunkRenderer(chunkId);
                    if (renderer)
                    {
                        uint32_t currentVersion = ++renderer->m_version;
                        std::lock_guard<std::mutex> lock(renderer->m_generationMutex);
                        renderer->GenerateMesh(currentVersion);
                    }
                }
            }
        }, highPriority);
    }

    void ChunkManager::SetBlockId(float x, float y, float z, BlockId blockId)
    {
        auto chunkId = WorldToChunkId(x, y, z);
        auto localPos = WorldToLocalChunkPos(x, y, z, chunkId);
        auto chunk = GetChunkData(chunkId);

        if (chunk && chunk->InBounds(localPos.x, localPos.y, localPos.z))
        {
            BlockId oldBlockId = chunk->Get(localPos.x, localPos.y, localPos.z);
            chunk->Set(localPos.x, localPos.y, localPos.z, blockId);

            static BlockRegistry& blockRegistry = BlockRegistry::GetInstance();
            auto& block = blockRegistry.GetBlock(blockId);

            // Get vector of chunks to remesh
            std::vector<std::shared_ptr<ChunkRenderer>> chunksToRemesh;
            chunksToRemesh.push_back(GetChunkRenderer(chunkId));

            // Remesh surrounding chunks if necessary
            if (localPos.x == 0)
            {
                auto renderer = GetChunkRenderer(chunkId.x - 1, chunkId.y, chunkId.z);
                chunksToRemesh.push_back(renderer);
            }
            else if (localPos.x == CHUNK_SIZE - 1)
            {
                auto renderer = GetChunkRenderer(chunkId.x + 1, chunkId.y, chunkId.z);
                chunksToRemesh.push_back(renderer);
            }
            if (localPos.y == 0)
            {
                auto renderer = GetChunkRenderer(chunkId.x, chunkId.y - 1, chunkId.z);
                chunksToRemesh.push_back(renderer);
            }
            else if (localPos.y == CHUNK_SIZE - 1)
            {
                auto renderer = GetChunkRenderer(chunkId.x, chunkId.y + 1, chunkId.z);
                chunksToRemesh.push_back(renderer);
            }
            if (localPos.z == 0)
            {
                auto renderer = GetChunkRenderer(chunkId.x, chunkId.y, chunkId.z - 1);
                chunksToRemesh.push_back(renderer);
            }
            else if (localPos.z == CHUNK_SIZE - 1)
            {
                auto renderer = GetChunkRenderer(chunkId.x, chunkId.y, chunkId.z + 1);
                chunksToRemesh.push_back(renderer);
            }

            // Start remesh job
            StartBatchChunkMeshJob(m_chunkThreadPool, chunksToRemesh, true);

            // Handle lighting updates
            if (block.lightEmitter)
            {
                StartLightAddJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, block.lightLevel, true);
            }
            else if (blockId == 0)
            {
                if (blockRegistry.GetBlock(oldBlockId).lightEmitter)
                    StartLightRemovalJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, true);
                else
                    StartLightBlockerRemovalJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, true);
            }
            else
                StartLightBlockerAddJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, true);

            // Start recalculate lighting job for current and surrounding chunks
            /*StartLightingRecalculationJob(m_chunkThreadPool, this, chunk, GetChunkRenderer(chunkId));

            for (int dx = -1; dx <= 1; dx++)
            {
                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dz = -1; dz <= 1; dz++)
                    {
                        if (dx == 0 && dy == 0 && dz == 0) continue;

                        auto neighborChunkId = chunkId + glm::ivec3(dx, dy, dz);
                        auto neighborChunkData = GetChunkData(neighborChunkId);
                        auto neighborChunkRenderer = GetChunkRenderer(neighborChunkId);
                        StartLightingRecalculationJob(m_chunkThreadPool, this, neighborChunkData, neighborChunkRenderer);
                    }
                }
            }*/
        }
    }

    std::shared_ptr<ChunkData> ChunkManager::GetChunkData(int x, int y, int z)
    {
        return GetChunkData({ x, y, z });
    }

    std::shared_ptr<ChunkData> ChunkManager::GetChunkDataAtPos(float x, float y, float z)
    {
        auto chunkId = WorldToChunkId(x, y, z);

        return GetChunkData(chunkId);
    }

    std::shared_ptr<ChunkRenderer> ChunkManager::GetChunkRenderer(int x, int y, int z)
    {
        return GetChunkRenderer({ x, y, z });
    }

    std::shared_ptr<ChunkRenderer> ChunkManager::GetChunkRendererAtPos(float x, float y, float z)
    {
        auto chunkId = WorldToChunkId(x, y, z);

        return GetChunkRenderer(chunkId);
    }

    std::shared_ptr<ChunkData> ChunkManager::GetChunkData(const glm::ivec3& id)
    {
        std::shared_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
        if (m_chunkData.find(id) != m_chunkData.end())
            return m_chunkData[id];

        return nullptr;
    }

    std::shared_ptr<ChunkRenderer> ChunkManager::GetChunkRenderer(const glm::ivec3& id)
    {
        std::shared_lock<std::shared_mutex> chunkRendererLock(m_chunkRendererMutex);
        if (m_chunkRenderers.find(id) != m_chunkRenderers.end())
            return m_chunkRenderers[id];

        return nullptr;
    }

    BlockId ChunkManager::GetBlockId(float x, float y, float z)
    {
        auto chunkId = WorldToChunkId(x, y, z);

        auto chunkData = GetChunkData(chunkId);
        if (!chunkData)
            return 0;

        auto localPos = WorldToLocalChunkPos(x, y, z, chunkId);

        return chunkData->Get(localPos.x, localPos.y, localPos.z);
    }

    void ChunkManager::Render()
    {
        {
            std::lock_guard<std::mutex> lock(m_chunkRendererDeletionMutex);
            while (!m_chunkRendererDeletionQueue.empty())
            {
                m_chunkRendererDeletionQueue.pop();
            }
        }

        m_chunkShader->Bind();
        m_chunkTexture->BindTexture(Texture::TEX00);
        {
            std::shared_lock<std::shared_mutex> lock(m_chunkRendererMutex);
            for (auto [id, chunk] : m_chunkRenderers)
            {
                chunk->Render();
            }
        }
    }

    std::shared_ptr<ChunkData> ChunkManager::GetOrGenerateChunkData(const glm::ivec3& id)
    {
        // Get chunk data if it already exists
        {
            std::shared_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
            auto it = m_chunkData.find(id);
            if (it != m_chunkData.end())
            {
                return it->second;
            }
        }

        // Check if chunk is within world bounds
        if (!((m_worldSizeX == 0 || (id.x >= -m_worldSizeX && id.x <= m_worldSizeX)) &&
            (m_worldMinY == 0 || id.y >= -m_worldMinY) && (m_worldMaxY == 0 || id.y <= m_worldMaxY) &&
            (m_worldSizeZ == 0 || (id.z >= -m_worldSizeZ && id.z <= m_worldSizeZ))))
        {
            return nullptr;
        }

        // Generate new chunk data
        auto chunkPos = id * CHUNK_SIZE;

        #ifdef DEBUG_MODE
        auto start = std::chrono::high_resolution_clock::now();
        #endif

        auto data = std::make_shared<ChunkData>(id);
        m_worldGen->Generate(data.get(), chunkPos);

        #ifdef DEBUG_MODE
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        m_chunkDataGenerated++;
        m_avgChunkDataGenTime = m_avgChunkDataGenTime + (duration.count() - m_avgChunkDataGenTime) / std::min(m_chunkDataGenerated, 1);
        #endif

        {
            std::unique_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
            m_chunkData[id] = data;
        }

        return data;
    }

    void ChunkManager::ChunkThread()
    {
        int prevXChunk = 1000;
        int prevYChunk = 1000;
        int prevZChunk = 1000;

        while (!m_chunkThreadShouldStop)
        {
            if (!m_camera) continue;

            int chunkX = m_camera->m_position.x < 0 ? (m_camera->m_position.x / CHUNK_SIZE) - 1 : m_camera->m_position.x / CHUNK_SIZE;
            int chunkY = m_camera->m_position.y < 0 ? (m_camera->m_position.y / CHUNK_SIZE) - 1 : m_camera->m_position.y / CHUNK_SIZE;
            int chunkZ = m_camera->m_position.z < 0 ? (m_camera->m_position.z / CHUNK_SIZE) - 1 : m_camera->m_position.z / CHUNK_SIZE;

            if (prevXChunk != chunkX || prevYChunk != chunkY || prevZChunk != chunkZ)
            {
                prevXChunk = chunkX;
                prevYChunk = chunkY;
                prevZChunk = chunkZ;

                // Clear queue
                std::queue<glm::ivec3> newQueue;
                std::swap(m_chunkQueue, newQueue);

                // Generate new queue
                m_chunkQueue.push({ chunkX, chunkY, chunkZ });

                for (int r = 0; r < m_renderDistance; r++)
                {
                    // Add middle chunks
                    for (int y = 0; y < m_renderHeight; y++)
                    {
                        m_chunkQueue.push({ chunkX,     chunkY + y, chunkZ + r });
                        m_chunkQueue.push({ chunkX + r, chunkY + y, chunkZ });
                        m_chunkQueue.push({ chunkX,     chunkY + y, chunkZ - r });
                        m_chunkQueue.push({ chunkX - r, chunkY + y, chunkZ });

                        if (y > 0)
                        {
                            m_chunkQueue.push({ chunkX,     chunkY - y, chunkZ + r });
                            m_chunkQueue.push({ chunkX + r, chunkY - y, chunkZ });
                            m_chunkQueue.push({ chunkX,     chunkY - y, chunkZ - r });
                            m_chunkQueue.push({ chunkX - r, chunkY - y, chunkZ });
                        }
                    }

                    // Add edges
                    for (int e = 1; e < r; e++)
                    {
                        for (int y = 0; y <= m_renderHeight; y++)
                        {
                            m_chunkQueue.push({ chunkX + e, chunkY + y, chunkZ + r });
                            m_chunkQueue.push({ chunkX - e, chunkY + y, chunkZ + r });

                            m_chunkQueue.push({ chunkX + r, chunkY + y, chunkZ + e });
                            m_chunkQueue.push({ chunkX + r, chunkY + y, chunkZ - e });

                            m_chunkQueue.push({ chunkX + e, chunkY + y, chunkZ - r });
                            m_chunkQueue.push({ chunkX - e, chunkY + y, chunkZ - r });

                            m_chunkQueue.push({ chunkX - r, chunkY + y, chunkZ + e });
                            m_chunkQueue.push({ chunkX - r, chunkY + y, chunkZ - e });

                            if (y > 0)
                            {
                                m_chunkQueue.push({ chunkX + e, chunkY - y, chunkZ + r });
                                m_chunkQueue.push({ chunkX - e, chunkY - y, chunkZ + r });

                                m_chunkQueue.push({ chunkX + r, chunkY - y, chunkZ + e });
                                m_chunkQueue.push({ chunkX + r, chunkY - y, chunkZ - e });

                                m_chunkQueue.push({ chunkX + e, chunkY - y, chunkZ - r });
                                m_chunkQueue.push({ chunkX - e, chunkY - y, chunkZ - r });

                                m_chunkQueue.push({ chunkX - r, chunkY - y, chunkZ + e });
                                m_chunkQueue.push({ chunkX - r, chunkY - y, chunkZ - e });
                            }
                        }
                    }

                    // Add corners
                    for (int y = 0; y <= m_renderHeight; y++)
                    {
                        m_chunkQueue.push({ chunkX + r, chunkY + y, chunkZ + r });
                        m_chunkQueue.push({ chunkX + r, chunkY + y, chunkZ - r });
                        m_chunkQueue.push({ chunkX - r, chunkY + y, chunkZ + r });
                        m_chunkQueue.push({ chunkX - r, chunkY + y, chunkZ - r });

                        if (y > 0)
                        {
                            m_chunkQueue.push({ chunkX + r, chunkY - y, chunkZ + r });
                            m_chunkQueue.push({ chunkX + r, chunkY - y, chunkZ - r });
                            m_chunkQueue.push({ chunkX - r, chunkY - y, chunkZ + r });
                            m_chunkQueue.push({ chunkX - r, chunkY - y, chunkZ - r });
                        }
                    }
                }

                // Delete chunk renderers out of range
                {
                    // Get chunk renderers out of range
                    std::vector<std::shared_ptr<ChunkRenderer>> chunksToDelete;

                    {
                        std::shared_lock<std::shared_mutex> chunkRenderLock(m_chunkRendererMutex);
                        for (auto& [id, chunk] : m_chunkRenderers)
                        {
                            glm::ivec3 id = chunk->m_chunkId;
                            if (std::abs(id.x - chunkX) > m_renderDistance ||
                                std::abs(id.y - chunkY) > m_renderHeight ||
                                std::abs(id.z - chunkZ) > m_renderDistance)
                            {
                                chunksToDelete.push_back(chunk);
                            }
                        }
                    }

                    // Add chunks to deletion queue
                    {
                        std::unique_lock<std::shared_mutex> chunkRenderLock(m_chunkRendererMutex);
                        for (auto& chunk : chunksToDelete)
                        {
                            m_chunkRenderers.erase(chunk->m_chunkId);
                        }
                    }
                    {
                        std::lock_guard<std::mutex> deleteLock(m_chunkRendererDeletionMutex);
                        for (auto& chunk : chunksToDelete)
                        {
                            m_chunkRendererDeletionQueue.push(chunk);
                        }
                    }
                }

                // Delete chunk data out of range
                {
                    // Get chunk data out of range
                    std::vector<glm::ivec3> chunkDataToDelete;

                    {
                        std::shared_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
                        std::shared_lock<std::shared_mutex> chunkRenderLock(m_chunkRendererMutex);
                        for (auto& [id, data] : m_chunkData)
                        {
                            glm::ivec3 id = data->id;
                            if (m_chunkRenderers.find(id) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x + 1, id.y, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x - 1, id.y, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y + 1, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y - 1, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y, id.z + 1 }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y, id.z - 1 }) == m_chunkRenderers.end())
                            {
                                chunkDataToDelete.push_back(id);
                            }
                        }
                    }

                    // Delete chunk data out of range
                    {
                        std::unique_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
                        for (auto& id : chunkDataToDelete)
                        {
                            m_chunkData.erase(id);
                        }
                    }
                }
            }

            if (!m_chunkQueue.empty())
            {
                auto id = m_chunkQueue.front();
                m_chunkQueue.pop();

                if (!((m_worldSizeX == 0 || (id.x >= -m_worldSizeX && id.x <= m_worldSizeX)) &&
                    (m_worldMinY == 0 || id.y >= -m_worldMinY) && (m_worldMaxY == 0 || id.y <= m_worldMaxY) &&
                    (m_worldSizeZ == 0 || (id.z >= -m_worldSizeZ && id.z <= m_worldSizeZ))))
                    continue;

                {
                    std::shared_lock<std::shared_mutex> chunkRendererLock(m_chunkRendererMutex);
                    if (m_chunkRenderers.find(id) != m_chunkRenderers.end())
                        continue;
                }

                // Create chunk data
                auto data = GetOrGenerateChunkData(id);
                std::unordered_set<glm::ivec3> chunksToRemesh;
                {
                    std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::lightingMutex);
                    chunksToRemesh = WillowVox::VoxelLighting::CalculateFullLighting(this, m_chunkData[id].get());
                }

                // Create chunk renderer
                auto chunk = std::make_shared<ChunkRenderer>(m_chunkData[id], id);

                // Set neighboring chunks
                chunk->SetSouthData(GetOrGenerateChunkData({ id.x, id.y, id.z + 1 }));
                chunk->SetNorthData(GetOrGenerateChunkData({ id.x, id.y, id.z - 1 }));
                chunk->SetEastData(GetOrGenerateChunkData({ id.x + 1, id.y, id.z }));
                chunk->SetWestData(GetOrGenerateChunkData({ id.x - 1, id.y, id.z }));
                chunk->SetUpData(GetOrGenerateChunkData({ id.x, id.y + 1, id.z }));
                chunk->SetDownData(GetOrGenerateChunkData({ id.x, id.y - 1, id.z }));

                // Generate chunk mesh data
                chunk->GenerateMesh();
                for (auto& chunkIdToRemesh : chunksToRemesh)
                {
                    auto rendererToRemesh = GetChunkRenderer(chunkIdToRemesh);
                    if (rendererToRemesh)
                    {
                        uint32_t currentVersion = ++rendererToRemesh->m_version;
                        std::lock_guard<std::mutex> lock(rendererToRemesh->m_generationMutex);
                        rendererToRemesh->GenerateMesh(currentVersion);
                    }
                }

                // Add chunk to map
                {
                    std::unique_lock<std::shared_mutex> lock(m_chunkRendererMutex);
                    m_chunkRenderers[id] = chunk;
                }
            }
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}