#pragma once

#include <wv/voxel_worlds/WorldGen.h>
#include <wv/voxel_worlds/ChunkRenderer.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <wv/core.h>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <queue>

namespace WillowVox
{
    class ChunkManager
    {
    public:
        ChunkManager(WorldGen* worldGen, int numChunkThreads, int worldSizeX = 0, int worldMinY = 0, int worldMaxY = 0, int worldSizeZ = 0);
        ~ChunkManager();

        // Gets chunk data at the specified chunk ID
        // If the chunk data does not exist, it will be generated
        // It is recommended to use TryGetChunkData when possible
        // as generation is slow and can freeze the current thread
        std::shared_ptr<ChunkData> GetChunkData(int x, int y, int z, LightingStage requiredLightingStage = LightingStage::LocalLightCalculated, uint8_t requiredWorldGenStage = 0);
        
        // Gets chunk data at the specified chunk ID
        // If the chunk data does not exist, it will be generated
        // It is recommended to use TryGetChunkData when possible
        // as generation is slow and can freeze the current thread
        std::shared_ptr<ChunkData> GetChunkData(const glm::ivec3& id, LightingStage requiredLightingStage = LightingStage::LocalLightCalculated, uint8_t requiredWorldGenStage = 0);
        
        // Gets chunk data at the specified world position
        // If the chunk data does not exist, it will be generated
        // It is recommended to use TryGetChunkData when possible
        // as generation is slow and can freeze the current thread
        std::shared_ptr<ChunkData> GetChunkDataAtPos(float x, float y, float z, LightingStage requiredLightingStage = LightingStage::LocalLightCalculated, uint8_t requiredWorldGenStage = 0);

        // Gets chunk data at the specified chunk ID
        // If the chunk data does not exist, nullptr is returned
        std::shared_ptr<ChunkData> TryGetChunkData(int x, int y, int z, LightingStage requiredLightingStage = LightingStage::LocalLightCalculated, uint8_t requiredWorldGenStage = 0);
        
        // Gets chunk data at the specified chunk ID
        // If the chunk data does not exist, nullptr is returned
        std::shared_ptr<ChunkData> TryGetChunkData(const glm::ivec3& id, LightingStage requiredLightingStage = LightingStage::LocalLightCalculated, uint8_t requiredWorldGenStage = 0);
        
        // Gets chunk data at the specified world position
        // If the chunk data does not exist, nullptr is returned
        std::shared_ptr<ChunkData> TryGetChunkDataAtPos(float x, float y, float z, LightingStage requiredLightingStage = LightingStage::LocalLightCalculated, uint8_t requiredWorldGenStage = 0);

        std::shared_ptr<ChunkRenderer> GetChunkRenderer(int x, int y, int z);
        std::shared_ptr<ChunkRenderer> GetChunkRenderer(const glm::ivec3& id);
        std::shared_ptr<ChunkRenderer> GetChunkRendererAtPos(float x, float y, float z);

        BlockId GetBlockId(float x, float y, float z);

        void SetBlockId(float x, float y, float z, BlockId blockId);

        void Render();

        void SetCamera(Camera* camera) { m_camera = camera; }
        void SetRenderDistance(int renderDistance, int renderHeight) { m_renderDistance = renderDistance; m_renderHeight = renderHeight; }

        inline glm::ivec3 WorldToBlockPos(float x, float y, float z)
        {
            int blockX = x < 0 ? floor(x) : x;
            int blockY = y < 0 ? floor(y) : y;
            int blockZ = z < 0 ? floor(z) : z;

            return { blockX, blockY, blockZ };
        }

        inline glm::ivec3 WorldToChunkId(float x, float y, float z)
        {
            auto blockPos = WorldToBlockPos(x, y, z);

            int chunkX = blockPos.x < 0 ? floor(blockPos.x / (float)CHUNK_SIZE) : blockPos.x / CHUNK_SIZE;
            int chunkY = blockPos.y < 0 ? floor(blockPos.y / (float)CHUNK_SIZE) : blockPos.y / CHUNK_SIZE;
            int chunkZ = blockPos.z < 0 ? floor(blockPos.z / (float)CHUNK_SIZE) : blockPos.z / CHUNK_SIZE;

            return { chunkX, chunkY, chunkZ };
        }
        inline glm::ivec3 BlockToChunkId(int x, int y, int z)
        {
            int chunkX = x < 0 ? floor(x / (float)CHUNK_SIZE) : x / CHUNK_SIZE;
            int chunkY = y < 0 ? floor(y / (float)CHUNK_SIZE) : y / CHUNK_SIZE;
            int chunkZ = z < 0 ? floor(z / (float)CHUNK_SIZE) : z / CHUNK_SIZE;

            return { chunkX, chunkY, chunkZ };
        }

        inline glm::ivec3 WorldToLocalChunkPos(float x, float y, float z, const glm::ivec3& id)
        {
            auto blockPos = WorldToBlockPos(x, y, z);
            return { blockPos.x - (id.x * CHUNK_SIZE),
                     blockPos.y - (id.y * CHUNK_SIZE),
                     blockPos.z - (id.z * CHUNK_SIZE) };
        }
        inline glm::ivec3 BlockToLocalChunkPos(int x, int y, int z, const glm::ivec3& id)
        {
            return { x - (id.x * CHUNK_SIZE),
                     y - (id.y * CHUNK_SIZE),
                     z - (id.z * CHUNK_SIZE) };
        }

        static void SaveChunkDataToFile(std::shared_ptr<ChunkData> chunk, const std::string& savePath);
        static std::shared_ptr<ChunkData> LoadChunkDataFromFile(const glm::ivec3& id, const std::string& savePath);

#ifdef DEBUG_MODE
        float m_avgChunkDataGenTime = 0.0f;
        int m_chunkDataGenerated = 0;
#endif

    private:
        std::shared_ptr<ChunkData> GetOrGenerateChunkData(const glm::ivec3& id);
        void ChunkThread();

        WorldGen* m_worldGen;

        Camera* m_camera = nullptr;
        int m_renderDistance, m_renderHeight;
        std::queue<glm::ivec3> m_chunkQueue;

        std::unordered_map<glm::ivec3, std::shared_ptr<ChunkData>> m_chunkData;
        std::shared_mutex m_chunkDataMutex;
        std::unordered_map <glm::ivec3, std::shared_ptr<ChunkRenderer>> m_chunkRenderers;
        std::shared_mutex m_chunkRendererMutex;

        std::queue<std::shared_ptr<ChunkRenderer>> m_chunkRendererDeletionQueue;
        std::mutex m_chunkRendererDeletionMutex;

        int m_worldSizeX, m_worldMinY, m_worldMaxY, m_worldSizeZ;

        std::shared_ptr<Shader> m_chunkShader;
        std::shared_ptr<Texture> m_chunkTexture;

        std::thread m_chunkThread;
        bool m_chunkThreadShouldStop = false;

        ThreadPool m_chunkThreadPool;
    };
}