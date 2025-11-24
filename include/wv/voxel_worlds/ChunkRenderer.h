#pragma once

#include <wv/voxel_worlds/ChunkData.h>
#include <wv/core.h>

namespace WillowVox
{
    class ChunkRenderer
    {
    public:
        struct ChunkVertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 texPos;
        };

        ChunkRenderer(std::shared_ptr<ChunkData> chunkData, const glm::ivec3& chunkId);
        ~ChunkRenderer();

        void SetNorthData(std::shared_ptr<ChunkData> data) { m_northChunkData = data; }
        void SetSouthData(std::shared_ptr<ChunkData> data) { m_southChunkData = data; }
        void SetEastData(std::shared_ptr<ChunkData> data) { m_eastChunkData = data; }
        void SetWestData(std::shared_ptr<ChunkData> data) { m_westChunkData = data; }
        void SetUpData(std::shared_ptr<ChunkData> data) { m_upChunkData = data; }
        void SetDownData(std::shared_ptr<ChunkData> data) { m_downChunkData = data; }

        void Render();

        void GenerateMesh();

#ifdef DEBUG_MODE
        static float m_avgMeshGenTime;
        static int m_meshesGenerated;
#endif

        glm::ivec3 m_chunkId;
        glm::vec3 m_chunkPos;
        
        std::atomic<bool> m_isGeneratingMesh;
        std::mutex m_generationMutex;

    private:
        std::shared_ptr<ChunkData> m_chunkData;
        std::unique_ptr<VertexArrayObject> m_vao;

        std::shared_ptr<ChunkData> m_northChunkData = nullptr;
        std::shared_ptr<ChunkData> m_southChunkData = nullptr;
        std::shared_ptr<ChunkData> m_eastChunkData = nullptr;
        std::shared_ptr<ChunkData> m_westChunkData = nullptr;
        std::shared_ptr<ChunkData> m_upChunkData = nullptr;
        std::shared_ptr<ChunkData> m_downChunkData = nullptr;

        std::shared_ptr<Shader> m_chunkShader;

        std::vector<ChunkVertex> m_vertices;
        std::vector<int> m_indices;
        bool m_dirty = true;
    };
}