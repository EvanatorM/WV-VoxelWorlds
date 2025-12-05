#include <wv/voxel_worlds/ChunkManager.h>
#include <wv/voxel_worlds/WorldGen.h>
#include <wv/voxel_worlds/VoxelLighting.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace WillowVox
{
    std::string SAVE_PATH = "./world";

    ChunkManager::ChunkManager(WorldGen* worldGen, int numChunkThreads, int worldSizeX, int worldMinY, int worldMaxY, int worldSizeZ)
        : m_worldGen(worldGen), m_worldSizeX(worldSizeX), m_worldMinY(worldMinY), m_worldMaxY(worldMaxY), m_worldSizeZ(worldSizeZ)
    {
        // Load assets
        auto& am = AssetManager::GetInstance();
        m_chunkShader = am.GetAsset<Shader>("chunk_shader");
        m_chunkTexture = am.GetAsset<Texture>("chunk_texture");

        // Start chunk thread pool
        m_chunkThreadPool.Start(numChunkThreads);

        // Start chunk thread
        m_chunkThread = std::thread(&ChunkManager::ChunkThread, this);

        if (!std::filesystem::exists(SAVE_PATH))
        {
            std::filesystem::create_directory(SAVE_PATH);
        }
    }

    ChunkManager::~ChunkManager()
    {
        // Stop chunk thread
        m_chunkThreadShouldStop = true;
        m_chunkThread.join();

        // Save all chunk data to files
        std::shared_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
        for (auto& [id, chunkData] : m_chunkData)
        {
            SaveChunkDataToFile(chunkData, SAVE_PATH);
        }
    }

    inline void StartChunkMeshJob(ThreadPool& pool, std::shared_ptr<ChunkRenderer> renderer, Priority priority = Priority::Medium)
    {
        if (!renderer)
            return;

        std::weak_ptr<ChunkRenderer> weakChunkPtr = renderer;
        pool.Enqueue([weakChunkPtr] {
            if (auto chunkPtr = weakChunkPtr.lock())
            {
                uint32_t currentVersion = ++chunkPtr->m_version;
                std::lock_guard<std::mutex> lock(chunkPtr->m_generationMutex);
                chunkPtr->GenerateMesh(currentVersion);
            }
        }, priority);
    }

    inline void StartBatchChunkMeshJob(ThreadPool& pool, std::vector<std::shared_ptr<ChunkRenderer>> renderers, Priority priority = Priority::Medium)
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

        pool.Enqueue([weakPtrs, versions, priority] {
            for (size_t i = 0; i < weakPtrs.size(); ++i)
            {
                auto& weakChunkPtr = weakPtrs[i];
                uint32_t currentVersion = versions[i];
                if (auto chunkPtr = weakChunkPtr.lock())
                {
                    std::lock_guard<std::mutex> lock(chunkPtr->m_generationMutex);
                    chunkPtr->GenerateMesh(currentVersion, true);
                }
            }

            for (auto& weakChunkPtr : weakPtrs)
            {
                if (auto chunkPtr = weakChunkPtr.lock())
                    chunkPtr->MarkDirty();
            }
        }, priority);
    }

    inline void StartLightingRecalculationJob(ThreadPool& pool, ChunkManager* chunkManager, std::shared_ptr<ChunkData> chunkData, std::shared_ptr<ChunkRenderer> renderer, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        std::weak_ptr<ChunkRenderer> weakChunkRendererPtr = renderer;
        pool.Enqueue([chunkManager, weakChunkDataPtr, weakChunkRendererPtr] {
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
        }, priority);
    }

    inline void StartLightAddJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, int lightLevel, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.Enqueue([&chunkManager, weakChunkDataPtr, x, y, z, lightLevel] {
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
        }, priority);
    }

    inline void StartLightRemovalJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.Enqueue([&chunkManager, weakChunkDataPtr, x, y, z] {
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
        }, priority);
    }

    inline void StartLightBlockerAddJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.Enqueue([&chunkManager, weakChunkDataPtr, x, y, z] {
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
        }, priority);
    }

    inline void StartLightBlockerRemovalJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.Enqueue([&chunkManager, weakChunkDataPtr, x, y, z] {
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
        }, priority);
    }

    inline void StartSkyLightBlockerAddJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.Enqueue([&chunkManager, weakChunkDataPtr, x, y, z] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::skyLightingMutex);
                auto chunksToRemesh = WillowVox::VoxelLighting::AddSkyLightBlocker(&chunkManager, chunkDataPtr.get(), x, y, z);

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
        }, priority);
    }

    inline void StartSkyLightBlockerRemovalJob(ThreadPool& pool, ChunkManager& chunkManager, std::shared_ptr<ChunkData> chunkData, int x, int y, int z, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        std::weak_ptr<ChunkData> weakChunkDataPtr = chunkData;
        pool.Enqueue([&chunkManager, weakChunkDataPtr, x, y, z] {
            if (auto chunkDataPtr = weakChunkDataPtr.lock())
            {
                std::lock_guard<std::mutex> lock(WillowVox::VoxelLighting::skyLightingMutex);
                auto chunksToRemesh = WillowVox::VoxelLighting::RemoveSkyLightBlocker(&chunkManager, chunkDataPtr.get(), x, y, z);

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
        }, priority);
    }

    inline void StartSaveChunkJob(ThreadPool& pool, std::shared_ptr<ChunkData> chunkData, Priority priority = Priority::Medium)
    {
        if (!chunkData)
            return;

        pool.Enqueue([chunkData] {
            ChunkManager::SaveChunkDataToFile(chunkData, SAVE_PATH);
        }, priority);
    }

    void ChunkManager::SetBlockId(float x, float y, float z, BlockId blockId)
    {
        auto chunkId = WorldToChunkId(x, y, z);
        auto localPos = WorldToLocalChunkPos(x, y, z, chunkId);
        auto chunk = GetChunkData(chunkId, LightingStage::ReadyForLighting);

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
            StartBatchChunkMeshJob(m_chunkThreadPool, chunksToRemesh, Priority::High);

            // Handle lighting updates
            if (block.lightEmitter)
            {
                StartLightAddJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, block.lightLevel, Priority::High);
                StartSkyLightBlockerAddJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
            }
            else if (blockId == 0)
            {
                if (blockRegistry.GetBlock(oldBlockId).lightEmitter)
                {
                    StartLightRemovalJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
                    StartSkyLightBlockerRemovalJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
                }
                else
                {
                    StartLightBlockerRemovalJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
                    StartSkyLightBlockerRemovalJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
                }
            }
            else
            {
                StartLightBlockerAddJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
                StartSkyLightBlockerAddJob(m_chunkThreadPool, *this, chunk, localPos.x, localPos.y, localPos.z, Priority::High);
            }
        }
    }

    std::shared_ptr<ChunkData> ChunkManager::GetChunkData(int x, int y, int z, LightingStage requiredLightingStage, uint8_t requiredWorldGenStage)
    {
        return GetChunkData({ x, y, z }, requiredLightingStage, requiredWorldGenStage);
    }

    std::shared_ptr<ChunkData> ChunkManager::GetChunkDataAtPos(float x, float y, float z, LightingStage requiredLightingStage, uint8_t requiredWorldGenStage)
    {
        auto chunkId = WorldToChunkId(x, y, z);

        return GetChunkData(chunkId, requiredLightingStage, requiredWorldGenStage);
    }

    std::shared_ptr<ChunkData> ChunkManager::GetChunkData(const glm::ivec3& id, LightingStage requiredLightingStage, uint8_t requiredWorldGenStage)
    {
        // Check if chunk is within world bounds
        if (!((m_worldSizeX == 0 || (id.x >= -m_worldSizeX && id.x <= m_worldSizeX)) &&
            (m_worldMinY == 0 || id.y >= -m_worldMinY) && (m_worldMaxY == 0 || id.y <= m_worldMaxY) &&
            (m_worldSizeZ == 0 || (id.z >= -m_worldSizeZ && id.z <= m_worldSizeZ))))
        {
            return nullptr;
        }
        
        std::shared_ptr<ChunkData> chunkData;

        // Try to find existing chunk data
        {
            std::shared_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
            auto it = m_chunkData.find(id);
            if (it != m_chunkData.end())
            {
                chunkData = it->second;
            }
        }
        
        // If chunk data doesn't exist, load or create it
        if (!chunkData)
        {
            // Load from file if it exists
            chunkData = LoadChunkDataFromFile(id, SAVE_PATH);
            if (!chunkData)
            {
                // Create new chunk data
                chunkData = std::make_shared<ChunkData>(id);
            }

            {
                std::unique_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
                m_chunkData[id] = chunkData;
            }
        }

        // Ensure chunk data meets required stages
        // If lighting stage is insufficient, perform world generation and lighting as needed
        if (requiredLightingStage > LightingStage::WorldGenInProgress)
        {
            // Finish world gen if necessary
            if (chunkData->lightingStage == LightingStage::WorldGenInProgress)
            {
                m_worldGen->Generate(chunkData.get(), id * CHUNK_SIZE, 255);
                chunkData->lightingStage = LightingStage::ReadyForLighting;
            }

            // Perform lighting if necessary
            if (requiredLightingStage == LightingStage::LocalLightCalculated && chunkData->lightingStage < LightingStage::LocalLightCalculated)
            {
                auto chunksToRemesh = WillowVox::VoxelLighting::CalculateFullLighting(this, chunkData.get());
                chunkData->lightingStage = LightingStage::LocalLightCalculated;

                // Schedule remesh jobs for affected chunks
                for (auto& chunkIdToRemesh : chunksToRemesh)
                {
                    auto rendererToRemesh = GetChunkRenderer(chunkIdToRemesh);
                    if (rendererToRemesh)
                    {
                        StartChunkMeshJob(m_chunkThreadPool, rendererToRemesh);
                    }
                }
            }
        }
        else if (chunkData->worldGenStage < requiredWorldGenStage)
        {
            m_worldGen->Generate(chunkData.get(), id * CHUNK_SIZE, requiredWorldGenStage);
            chunkData->worldGenStage = requiredWorldGenStage;
        }
        
        return chunkData;
    }

    std::shared_ptr<ChunkData> ChunkManager::TryGetChunkData(int x, int y, int z, LightingStage requiredLightingStage, uint8_t requiredWorldGenStage)
    {
        return TryGetChunkData({ x, y, z }, requiredLightingStage, requiredWorldGenStage);
    }

    std::shared_ptr<ChunkData> ChunkManager::TryGetChunkDataAtPos(float x, float y, float z, LightingStage requiredLightingStage, uint8_t requiredWorldGenStage)
    {
        auto chunkId = WorldToChunkId(x, y, z);

        return TryGetChunkData(chunkId, requiredLightingStage, requiredWorldGenStage);
    }

    std::shared_ptr<ChunkData> ChunkManager::TryGetChunkData(const glm::ivec3& id, LightingStage requiredLightingStage, uint8_t requiredWorldGenStage)
    {
        std::shared_lock<std::shared_mutex> chunkDataLock(m_chunkDataMutex);
        auto it = m_chunkData.find(id);
        if (it == m_chunkData.end())
        {
            return nullptr;
        }

        if (it->second->lightingStage < requiredLightingStage)
            return nullptr;
        if (requiredLightingStage > LightingStage::WorldGenInProgress && it->second->worldGenStage < requiredWorldGenStage)
            return nullptr;
        
        return it->second;
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

    
    void ChunkManager::SaveChunkDataToFile(std::shared_ptr<ChunkData> chunk, const std::string& savePath)
    {
        // File Format:
        // [2 bytes] Version number (uint16_t)
        // [CHUNK_VOLUME bytes] BlockId array
        // [CHUNK_VOLUME bytes] Light level array
        // [CHUNK_VOLUME bytes] Sky light level array
        // [1 byte ] World generation stage (uint8_t)
        // [1 byte ] Lighting stage (uint8_t)

        // Construct file path
        std::string filePath = savePath + "/chunk_" +
            std::to_string(chunk->id.x) + "_" +
            std::to_string(chunk->id.y) + "_" +
            std::to_string(chunk->id.z) + ".dat";
        
        // Open file for binary writing
        std::ofstream outFile(filePath, std::ios::binary);
        if (outFile.is_open())
        {
            outFile.write(reinterpret_cast<const char*>(&CHUNK_DATA_VERSION), sizeof(CHUNK_DATA_VERSION));
            outFile.write(reinterpret_cast<const char*>(chunk->voxels), sizeof(chunk->voxels));
            outFile.write(reinterpret_cast<const char*>(chunk->lightLevels), sizeof(chunk->lightLevels));
            outFile.write(reinterpret_cast<const char*>(chunk->skyLightLevels), sizeof(chunk->skyLightLevels));
            outFile.write(reinterpret_cast<const char*>(&chunk->worldGenStage), sizeof(chunk->worldGenStage));
            uint8_t lightingStage = static_cast<uint8_t>(chunk->lightingStage);
            outFile.write(reinterpret_cast<const char*>(&lightingStage), sizeof(lightingStage));
            outFile.close();
        }
        else
        {
            Logger::Error("Failed to write chunk data (%d %d %d) to file: %s", chunk->id.x, chunk->id.y, chunk->id.z, filePath.c_str());
        }
    }
    
    std::shared_ptr<ChunkData> ChunkManager::LoadChunkDataFromFile(const glm::ivec3& id, const std::string& savePath)
    {
        // File Format:
        // [2 bytes] Version number (uint16_t)
        // [CHUNK_VOLUME bytes] BlockId array
        // [CHUNK_VOLUME bytes] Light level array
        // [CHUNK_VOLUME bytes] Sky light level array
        // [1 byte ] World generation stage (uint8_t)
        // [1 byte ] Lighting stage (uint8_t)

        // Construct file path
        std::string filePath = savePath + "/chunk_" +
            std::to_string(id.x) + "_" +
            std::to_string(id.y) + "_" +
            std::to_string(id.z) + ".dat";

        // Open file for binary reading
        std::ifstream inFile(filePath, std::ios::binary);
        if (inFile.is_open())
        {
            uint16_t version;
            inFile.read(reinterpret_cast<char*>(&version), sizeof(version));
            if (version != CHUNK_DATA_VERSION)
            {
                Logger::Warn("Chunk data version mismatch (%d != %d) for chunk (%d %d %d). Regenerating chunk.", version, CHUNK_DATA_VERSION, id.x, id.y, id.z);
                inFile.close();
                return nullptr;
            }

            auto chunkData = std::make_shared<ChunkData>(id);
            inFile.read(reinterpret_cast<char*>(chunkData->voxels), sizeof(chunkData->voxels));
            inFile.read(reinterpret_cast<char*>(chunkData->lightLevels), sizeof(chunkData->lightLevels));
            inFile.read(reinterpret_cast<char*>(chunkData->skyLightLevels), sizeof(chunkData->skyLightLevels));
            inFile.read(reinterpret_cast<char*>(&chunkData->worldGenStage), sizeof(chunkData->worldGenStage));
            uint8_t lightingStage;
            inFile.read(reinterpret_cast<char*>(&lightingStage), sizeof(lightingStage));
            chunkData->lightingStage = static_cast<LightingStage>(lightingStage);

            inFile.close();
            return chunkData;
        }
        else
        {
            return nullptr;
        }
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
                            if (m_chunkRenderers.find(id) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x + 1, id.y, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x - 1, id.y, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y + 1, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y - 1, id.z }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y, id.z + 1 }) == m_chunkRenderers.end() &&
                                m_chunkRenderers.find({ id.x, id.y, id.z - 1 }) == m_chunkRenderers.end())
                            {
                                chunkDataToDelete.push_back(id);
                                break; // Only delete one chunk at a time for now to load new chunks faster (this is a temporary solution)
                            }
                        }
                    }

                    // Save chunk data out of range
                    {
                        for (auto& id : chunkDataToDelete)
                        {
                            auto chunkData = GetChunkData(id);
                            if (chunkData)
                            {
                                SaveChunkDataToFile(chunkData, SAVE_PATH);
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
                auto data = GetChunkData(id);

                // Create chunk renderer
                auto chunk = std::make_shared<ChunkRenderer>(m_chunkData[id], id);

                // Set neighboring chunks
                chunk->SetSouthData(GetChunkData({ id.x, id.y, id.z + 1 }));
                chunk->SetNorthData(GetChunkData({ id.x, id.y, id.z - 1 }));
                chunk->SetEastData(GetChunkData({ id.x + 1, id.y, id.z }));
                chunk->SetWestData(GetChunkData({ id.x - 1, id.y, id.z }));
                chunk->SetUpData(GetChunkData({ id.x, id.y + 1, id.z }));
                chunk->SetDownData(GetChunkData({ id.x, id.y - 1, id.z }));

                // Generate chunk mesh data
                chunk->GenerateMesh();

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