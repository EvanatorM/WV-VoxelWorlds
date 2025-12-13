#include <wv/voxel_worlds/ChunkRenderer.h>
#include <wv/voxel_worlds/BlockRegistry.h>
#include <chrono>

namespace WillowVox
{
#ifdef DEBUG_MODE
    float ChunkRenderer::m_avgMeshGenTime = 0;
    int ChunkRenderer::m_meshesGenerated = 0;
#endif

    ChunkRenderer::ChunkRenderer(std::shared_ptr<ChunkData> chunkData, const glm::ivec3& chunkId)
        : m_chunkData(chunkData), m_chunkId(chunkId), m_chunkPos(chunkId* CHUNK_SIZE)
    {
        auto& am = AssetManager::GetInstance();
        m_chunkShader = am.GetAsset<Shader>("chunk_shader");
    }

    ChunkRenderer::~ChunkRenderer()
    {
        //Logger::Log("Destroying ChunkRenderer at (%d, %d, %d)", m_chunkId.x, m_chunkId.y, m_chunkId.z);
    }

    void ChunkRenderer::Render()
    {
        // Create vao if it doesn't exist
        if (!m_vao)
        {
            m_vao = std::make_unique<VertexArrayObject>();
            m_vao->SetAttribPointer(0, 3, VertexBufferAttribType::FLOAT32, false, sizeof(ChunkVertex), offsetof(ChunkVertex, px));
            m_vao->SetAttribPointer(1, 3, VertexBufferAttribType::FLOAT32, false, sizeof(ChunkVertex), offsetof(ChunkVertex, nx));
            m_vao->SetAttribPointer(2, 2, VertexBufferAttribType::FLOAT32, false, sizeof(ChunkVertex), offsetof(ChunkVertex, u));
            m_vao->SetAttribPointer(3, 1, VertexBufferAttribType::UINT16, false, sizeof(ChunkVertex), offsetof(ChunkVertex, lightData));
        }

        // Buffer data is dirty
        if (m_dirty)
        {
            std::lock_guard<std::mutex> lock(m_meshDataMutex);
            m_vao->BufferVertexData(m_vertices.size() * sizeof(ChunkVertex), m_vertices.data());
            m_vao->BufferElementData(ElementBufferAttribType::UINT32, m_indices.size(), m_indices.data());
            m_dirty = false;
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m_chunkPos);
        m_chunkShader->SetMat4("model", model);

        m_vao->Draw();
    }

    inline void AddIndices(std::vector<int>& indices, int& vertexCount)
    {
        indices.push_back(vertexCount + 0);
        indices.push_back(vertexCount + 2);
        indices.push_back(vertexCount + 1);

        indices.push_back(vertexCount + 1);
        indices.push_back(vertexCount + 2);
        indices.push_back(vertexCount + 3);
        vertexCount += 4;
    }

    void ChunkRenderer::GenerateMesh(uint32_t currentVersion, bool batch)
    {
        if (currentVersion == 0)
            currentVersion = m_version;
        if (currentVersion != m_version)
            return; // Abort lighting calculation if version has changed

        #ifdef DEBUG_MODE
        auto start = std::chrono::high_resolution_clock::now();
        #endif

        std::vector<ChunkVertex> vertices;
        std::vector<int> indices;
        int vertexCount = 0;

        auto& blockRegistry = BlockRegistry::GetInstance();

        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                if (currentVersion != m_version)
                    return; // Abort mesh generation if version has changed

                for (int y = 0; y < CHUNK_SIZE; y++)
                {
                    BlockId id = m_chunkData->Get(x, y, z);
                    if (id == 0)
                        continue;

                    auto& block = blockRegistry.GetBlock(id);

                    // South Face
                    {
                        bool south;
                        if (z + 1 >= CHUNK_SIZE)
                        {
                            if (m_southChunkData)
                                south = m_southChunkData->Get(x, y, 0) == 0;
                            else
                                south = true;
                        }
                        else
                            south = m_chunkData->Get(x, y, z + 1) == 0;
                        if (south)
                        {
                            uint16_t lightData;
                            if (z + 1 >= CHUNK_SIZE)
                            {
                                if (m_southChunkData)
                                    lightData = m_southChunkData->GetLightData(x, y, 0);
                                else
                                    lightData = 0;
                            }
                            else
                                lightData = m_chunkData->GetLightData(x, y, z + 1);

                            // South Face
                            vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 0, 0, 1, block.sideTexMinX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, 0, 0, 1, block.sideTexMaxX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 0, 0, 1, block.sideTexMinX, block.sideTexMaxY, lightData });
                            vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, 0, 0, 1, block.sideTexMaxX, block.sideTexMaxY, lightData });

                            AddIndices(indices, vertexCount);
                        }
                    }

                    // North Face
                    {
                        bool north;
                        if (z < 1)
                        {
                            if (m_northChunkData)
                                north = m_northChunkData->Get(x, y, CHUNK_SIZE - 1) == 0;
                            else
                                north = true;
                        }
                        else
                            north = m_chunkData->Get(x, y, z - 1) == 0;
                        if (north)
                        {
                            uint16_t lightData;
                            if (z < 1)
                            {
                                if (m_northChunkData)
                                    lightData = m_northChunkData->GetLightData(x, y, CHUNK_SIZE - 1);
                                else
                                    lightData = 0;
                            }
                            else
                                lightData = m_chunkData->GetLightData(x, y, z - 1);

                            // North Face
                            vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, 0, 0, -1, block.sideTexMinX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 0, 0, -1, block.sideTexMaxX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, 0, 0, -1, block.sideTexMinX, block.sideTexMaxY, lightData });
                            vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 0, 0, -1, block.sideTexMaxX, block.sideTexMaxY, lightData });

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // East Face
                    {
                        bool east;
                        if (x + 1 >= CHUNK_SIZE)
                        {
                            if (m_eastChunkData)
                                east = m_eastChunkData->Get(0, y, z) == 0;
                            else
                                east = true;
                        }
                        else
                            east = m_chunkData->Get(x + 1, y, z) == 0;
                        if (east)
                        {
                            uint16_t lightData;
                            if (x + 1 >= CHUNK_SIZE)
                            {
                                if (m_eastChunkData)
                                    lightData = m_eastChunkData->GetLightData(0, y, z);
                                else
                                    lightData = 0;
                            }
                            else
                                lightData = m_chunkData->GetLightData(x + 1, y, z);

                            // East Face
                            vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, -1, 0, 0, block.sideTexMinX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, -1, 0, 0, block.sideTexMaxX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, -1, 0, 0, block.sideTexMinX, block.sideTexMaxY, lightData });
                            vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, -1, 0, 0, block.sideTexMaxX, block.sideTexMaxY, lightData });

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // West Face
                    {
                        bool west;
                        if (x < 1)
                        {
                            if (m_westChunkData)
                                west = m_westChunkData->Get(CHUNK_SIZE - 1, y, z) == 0;
                            else
                                west = true;
                        }
                        else
                            west = m_chunkData->Get(x - 1, y, z) == 0;
                        if (west)
                        {
                            uint16_t lightData;
                            if (x < 1)
                            {
                                if (m_westChunkData)
                                    lightData = m_westChunkData->GetLightData(CHUNK_SIZE - 1, y, z);
                                else
                                    lightData = 0;
                            }
                            else
                                lightData = m_chunkData->GetLightData(x - 1, y, z);

                            // West Face
                            vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 1, 0, 0, block.sideTexMinX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 1, 0, 0, block.sideTexMaxX, block.sideTexMinY, lightData });
                            vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 1, 0, 0, block.sideTexMinX, block.sideTexMaxY, lightData });
                            vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 1, 0, 0, block.sideTexMaxX, block.sideTexMaxY, lightData });

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // Up Face
                    {
                        bool up;
                        if (y + 1 >= CHUNK_SIZE)
                        {
                            if (m_upChunkData)
                                up = m_upChunkData->Get(x, 0, z) == 0;
                            else
                                up = true;
                        }
                        else
                            up = m_chunkData->Get(x, y + 1, z) == 0;
                        if (up)
                        {
                            uint16_t lightData;
                            if (y + 1 >= CHUNK_SIZE)
                            {
                                if (m_upChunkData)
                                    lightData = m_upChunkData->GetLightData(x, 0, z);
                                else
                                    lightData = 0;
                            }
                            else
                                lightData = m_chunkData->GetLightData(x, y + 1, z);

                            // Up Face
                            vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 0, 1, 0, block.topTexMinX, block.topTexMinY, lightData });
                            vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, 0, 1, 0, block.topTexMaxX, block.topTexMinY, lightData });
                            vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 0, 1, 0, block.topTexMinX, block.topTexMaxY, lightData });
                            vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, 0, 1, 0, block.topTexMaxX, block.topTexMaxY, lightData });

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // Down Face
                    {
                        bool down;
                        if (y < 1)
                        {
                            if (m_downChunkData)
                                down = m_downChunkData->Get(x, CHUNK_SIZE - 1, z) == 0;
                            else
                                down = true;
                        }
                        else
                            down = m_chunkData->Get(x, y - 1, z) == 0;
                        if (down)
                        {
                            uint16_t lightData;
                            if (y < 1)
                            {
                                if (m_downChunkData)
                                    lightData = m_downChunkData->GetLightData(x, CHUNK_SIZE - 1, z);
                                else
                                    lightData = 0;
                            }
                            else
                                lightData = m_chunkData->GetLightData(x, y - 1, z);

                            // Down Face
                            vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, 0, -1, 0, block.bottomTexMinX, block.bottomTexMinY, lightData });
                            vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 0, -1, 0, block.bottomTexMaxX, block.bottomTexMinY, lightData });
                            vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, 0, -1, 0, block.bottomTexMinX, block.bottomTexMaxY, lightData });
                            vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 0, -1, 0, block.bottomTexMaxX, block.bottomTexMaxY, lightData });

                            AddIndices(indices, vertexCount);
                        }
                    }
                }
            }
        }


        {
            std::lock_guard<std::mutex> lock(m_meshDataMutex);
            std::swap(m_vertices, vertices);
            std::swap(m_indices, indices);
        }

        if (!batch)
            m_dirty = true;

        #ifdef DEBUG_MODE
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        m_meshesGenerated++;
        m_avgMeshGenTime = m_avgMeshGenTime + (duration.count() - m_avgMeshGenTime) / std::min(m_meshesGenerated, 1);
        #endif
    }
}