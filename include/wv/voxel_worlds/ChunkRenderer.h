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
            float px, py, pz;
            float nx, ny, nz;
            float u, v;
            uint16_t lightData;
        };

        ChunkRenderer(std::shared_ptr<ChunkData> chunkData, const glm::ivec3& chunkId);
        ~ChunkRenderer();

        // Expects the chunk id axes to be in order of x, y, z
        void SetNeighboringChunks(const std::array<std::shared_ptr<ChunkData>, 27>& chunks) { m_neighboringChunkData = chunks; }

        void Render();

        void GenerateMesh(uint32_t currentVersion = 0, bool batch = false);
        void MarkDirty() { m_dirty = true; }

        static void SetSmoothLighting(bool value) { smoothLighting = value; }

#ifdef DEBUG_MODE
        static float m_avgMeshGenTime;
        static int m_meshesGenerated;
#endif

        glm::ivec3 m_chunkId;
        glm::vec3 m_chunkPos;
        
        std::mutex m_generationMutex;
        std::mutex m_meshDataMutex;

        std::atomic<uint32_t> m_version = 0;

    private:
        std::shared_ptr<ChunkData> m_chunkData;
        std::unique_ptr<VertexArrayObject> m_vao;

        std::array<std::shared_ptr<ChunkData>, 27> m_neighboringChunkData;

        std::shared_ptr<Shader> m_chunkShader;

        std::vector<ChunkVertex> m_vertices;
        std::vector<int> m_indices;
        bool m_dirty = true;

        static bool smoothLighting;
    };
}