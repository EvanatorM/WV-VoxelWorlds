#include <wv/voxel_worlds/ChunkRenderer.h>
#include <wv/voxel_worlds/BlockRegistry.h>
#include <chrono>

namespace WillowVox
{
#ifdef DEBUG_MODE
    float ChunkRenderer::m_avgMeshGenTime = 0;
    int ChunkRenderer::m_meshesGenerated = 0;
#endif

    bool ChunkRenderer::smoothLighting = true;

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

    inline std::shared_ptr<ChunkData> GetChunk(const std::array<std::shared_ptr<ChunkData>, 27>& chunks, int& x, int& y, int& z)
    {
        // Chunk coords
        int cx = 1;
        int cy = 1;
        int cz = 1;

        if (x < 0)
        {
            x = CHUNK_SIZE - 1;
            cx--;
        }
        else if (x > CHUNK_SIZE - 1)
        {
            x = 0;
            cx++;
        }
        
        if (y < 0)
        {
            y = CHUNK_SIZE - 1;
            cy--;
        }
        else if (y > CHUNK_SIZE - 1)
        {
            y = 0;
            cy++;
        }

        if (z < 0)
        {
            z = CHUNK_SIZE - 1;
            cz--;
        }
        else if (z > CHUNK_SIZE - 1)
        {
            z = 0;
            cz++;
        }

        // Assumes order x, y, z
        return chunks[cz + cy * 3 + cx * 9];
    }

    struct templight {
        int r, g, b, s;
        bool use;

        static templight Avg(const templight& l1, const templight& l2, const templight& l3, const templight& l4)
        {
            templight l;
            l.r = 0;
            l.g = 0;
            l.b = 0;
            l.s = 0;
            int n = 0;
            if (l1.use)
            {
                l.r += l1.r;
                l.g += l1.g;
                l.b += l1.b;
                l.s += l1.s;
                n++;
            }
            if (l2.use)
            {
                l.r += l2.r;
                l.g += l2.g;
                l.b += l2.b;
                l.s += l2.s;
                n++;
            }
            if (l3.use)
            {
                l.r += l3.r;
                l.g += l3.g;
                l.b += l3.b;
                l.s += l3.s;
                n++;
            }
            if (l4.use)
            {
                l.r += l4.r;
                l.g += l4.g;
                l.b += l4.b;
                l.s += l4.s;
                n++;
            }

            if (n > 0)
            {
                l.r /= n;
                l.g /= n;
                l.b /= n;
                l.s /= n;
            }

            return l;
        }

        uint16_t GetPackedLightData()
        {
            uint16_t lightData = 0;
            lightData = (lightData & 0x0FFF) | (s << 12);
            lightData = (lightData & 0xF0FF) | (r << 8);
            lightData = (lightData & 0xFF0F) | (g << 4);
            lightData = (lightData & 0xFFF0) | (b << 0);
            return lightData;
        }
    };

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
                        int bx = x, by = y, bz = z + 1;
                        auto nData = GetChunk(m_neighboringChunkData, bx, by, bz);
                        bool south = nData ? nData->Get(bx, by, bz) == 0 : true;
                        if (south)
                        {
                            if (smoothLighting)
                            {
                                // Get 9 top blocks
                                std::array<templight, 9> lights;
                                {
                                    int lz = z + 1;
                                    int i = 0;
                                    for (int ly = y - 1; ly <= y + 1; ly++)
                                    {
                                        for (int lx = x - 1; lx <= x + 1; lx++, i++)
                                        {
                                            int cx = lx, cy = ly, cz = lz;
                                            auto lData = GetChunk(m_neighboringChunkData, cx, cy, cz);
                                            if (lData && lData->Get(cx, cy, cz) == 0)
                                            {
                                                lights[i].r = lData->GetRedLight(cx, cy, cz);
                                                lights[i].g = lData->GetGreenLight(cx, cy, cz);
                                                lights[i].b = lData->GetBlueLight(cx, cy, cz);
                                                lights[i].s = lData->GetSkyLight(cx, cy, cz);
                                                lights[i].use = true;
                                            }
                                            else
                                                lights[i].use = false;
                                        }
                                    }
                                }

                                // Create face
                                templight avg = templight::Avg(lights[0], lights[1], lights[3], lights[4]);
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 0, 0, 1, block.sideTexMinX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[1], lights[2], lights[4], lights[5]);
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, 0, 0, 1, block.sideTexMaxX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[3], lights[4], lights[6], lights[7]);
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 0, 0, 1, block.sideTexMinX, block.sideTexMaxY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[4], lights[5], lights[7], lights[8]);
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, 0, 0, 1, block.sideTexMaxX, block.sideTexMaxY, avg.GetPackedLightData() });
                            }
                            else
                            {
                                uint16_t lightData = nData ? nData->GetLightData(bx, by, bz) : 0;

                                // South Face
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 0, 0, 1, block.sideTexMinX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, 0, 0, 1, block.sideTexMaxX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 0, 0, 1, block.sideTexMinX, block.sideTexMaxY, lightData });
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, 0, 0, 1, block.sideTexMaxX, block.sideTexMaxY, lightData });
                            }

                            AddIndices(indices, vertexCount);
                        }
                    }

                    // North Face
                    {
                        int bx = x, by = y, bz = z - 1;
                        auto nData = GetChunk(m_neighboringChunkData, bx, by, bz);
                        bool north = nData ? nData->Get(bx, by, bz) == 0 : true;
                        if (north)
                        {
                            if (smoothLighting)
                            {
                                // Get 9 top blocks
                                std::array<templight, 9> lights;
                                {
                                    int lz = z - 1;
                                    int i = 0;
                                    for (int ly = y - 1; ly <= y + 1; ly++)
                                    {
                                        for (int lx = x - 1; lx <= x + 1; lx++, i++)
                                        {
                                            int cx = lx, cy = ly, cz = lz;
                                            auto lData = GetChunk(m_neighboringChunkData, cx, cy, cz);
                                            if (lData && lData->Get(cx, cy, cz) == 0)
                                            {
                                                lights[i].r = lData->GetRedLight(cx, cy, cz);
                                                lights[i].g = lData->GetGreenLight(cx, cy, cz);
                                                lights[i].b = lData->GetBlueLight(cx, cy, cz);
                                                lights[i].s = lData->GetSkyLight(cx, cy, cz);
                                                lights[i].use = true;
                                            }
                                            else
                                                lights[i].use = false;
                                        }
                                    }
                                }

                                // Create face
                                templight avg = templight::Avg(lights[1], lights[2], lights[4], lights[5]);
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, 0, 0, -1, block.sideTexMinX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[0], lights[1], lights[3], lights[4]);
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 0, 0, -1, block.sideTexMaxX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[4], lights[5], lights[7], lights[8]);
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, 0, 0, -1, block.sideTexMinX, block.sideTexMaxY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[3], lights[4], lights[6], lights[7]);
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 0, 0, -1, block.sideTexMaxX, block.sideTexMaxY, avg.GetPackedLightData() });
                            }
                            else
                            {
                                uint16_t lightData = nData ? nData->GetLightData(bx, by, bz) : 0;

                                // North Face
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, 0, 0, -1, block.sideTexMinX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 0, 0, -1, block.sideTexMaxX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, 0, 0, -1, block.sideTexMinX, block.sideTexMaxY, lightData });
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 0, 0, -1, block.sideTexMaxX, block.sideTexMaxY, lightData });
                            }

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // East Face
                    {
                        int bx = x + 1, by = y, bz = z;
                        auto nData = GetChunk(m_neighboringChunkData, bx, by, bz);
                        bool east = nData ? nData->Get(bx, by, bz) == 0 : true;
                        if (east)
                        {
                            if (smoothLighting)
                            {
                                // Get 9 top blocks
                                std::array<templight, 9> lights;
                                {
                                    int lx = x + 1;
                                    int i = 0;
                                    for (int ly = y - 1; ly <= y + 1; ly++)
                                    {
                                        for (int lz = z - 1; lz <= z + 1; lz++, i++)
                                        {
                                            int cx = lx, cy = ly, cz = lz;
                                            auto lData = GetChunk(m_neighboringChunkData, cx, cy, cz);
                                            if (lData && lData->Get(cx, cy, cz) == 0)
                                            {
                                                lights[i].r = lData->GetRedLight(cx, cy, cz);
                                                lights[i].g = lData->GetGreenLight(cx, cy, cz);
                                                lights[i].b = lData->GetBlueLight(cx, cy, cz);
                                                lights[i].s = lData->GetSkyLight(cx, cy, cz);
                                                lights[i].use = true;
                                            }
                                            else
                                                lights[i].use = false;
                                        }
                                    }
                                }

                                // Create face
                                templight avg = templight::Avg(lights[1], lights[2], lights[4], lights[5]);
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, -1, 0, 0, block.sideTexMinX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[0], lights[1], lights[3], lights[4]);
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, -1, 0, 0, block.sideTexMaxX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[4], lights[5], lights[7], lights[8]);
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, -1, 0, 0, block.sideTexMinX, block.sideTexMaxY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[3], lights[4], lights[6], lights[7]);
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, -1, 0, 0, block.sideTexMaxX, block.sideTexMaxY, avg.GetPackedLightData() });
                            }
                            else
                            {
                                uint16_t lightData = nData ? nData->GetLightData(bx, by, bz) : 0;

                                // East Face
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, -1, 0, 0, block.sideTexMinX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, -1, 0, 0, block.sideTexMaxX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, -1, 0, 0, block.sideTexMinX, block.sideTexMaxY, lightData });
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, -1, 0, 0, block.sideTexMaxX, block.sideTexMaxY, lightData });
                            }

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // West Face
                    {
                        int bx = x - 1, by = y, bz = z;
                        auto nData = GetChunk(m_neighboringChunkData, bx, by, bz);
                        bool west = nData ? nData->Get(bx, by, bz) == 0 : true;
                        if (west)
                        {
                            if (smoothLighting)
                            {
                                // Get 9 top blocks
                                std::array<templight, 9> lights;
                                {
                                    int lx = x - 1;
                                    int i = 0;
                                    for (int ly = y - 1; ly <= y + 1; ly++)
                                    {
                                        for (int lz = z - 1; lz <= z + 1; lz++, i++)
                                        {
                                            int cx = lx, cy = ly, cz = lz;
                                            auto lData = GetChunk(m_neighboringChunkData, cx, cy, cz);
                                            if (lData && lData->Get(cx, cy, cz) == 0)
                                            {
                                                lights[i].r = lData->GetRedLight(cx, cy, cz);
                                                lights[i].g = lData->GetGreenLight(cx, cy, cz);
                                                lights[i].b = lData->GetBlueLight(cx, cy, cz);
                                                lights[i].s = lData->GetSkyLight(cx, cy, cz);
                                                lights[i].use = true;
                                            }
                                            else
                                                lights[i].use = false;
                                        }
                                    }
                                }

                                // Create face
                                templight avg = templight::Avg(lights[0], lights[1], lights[3], lights[4]);
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 1, 0, 0, block.sideTexMinX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[1], lights[2], lights[4], lights[5]);
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 1, 0, 0, block.sideTexMaxX, block.sideTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[3], lights[4], lights[6], lights[7]);
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 1, 0, 0, block.sideTexMinX, block.sideTexMaxY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[4], lights[5], lights[7], lights[8]);
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 1, 0, 0, block.sideTexMaxX, block.sideTexMaxY, avg.GetPackedLightData() });
                            }
                            else
                            {
                                uint16_t lightData = nData ? nData->GetLightData(bx, by, bz) : 0;

                                // West Face
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 1, 0, 0, block.sideTexMinX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 1, 0, 0, block.sideTexMaxX, block.sideTexMinY, lightData });
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 1, 0, 0, block.sideTexMinX, block.sideTexMaxY, lightData });
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 1, 0, 0, block.sideTexMaxX, block.sideTexMaxY, lightData });
                            }

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // Up Face
                    {
                        int bx = x, by = y + 1, bz = z;
                        auto nData = GetChunk(m_neighboringChunkData, bx, by, bz);
                        bool up = nData ? nData->Get(bx, by, bz) == 0 : true;
                        if (up)
                        {
                            if (smoothLighting)
                            {
                                // Get 9 top blocks
                                std::array<templight, 9> lights;
                                {
                                    int ly = y + 1;
                                    int i = 0;
                                    for (int lz = z - 1; lz <= z + 1; lz++)
                                    {
                                        for (int lx = x - 1; lx <= x + 1; lx++, i++)
                                        {
                                            int cx = lx, cy = ly, cz = lz;
                                            auto lData = GetChunk(m_neighboringChunkData, cx, cy, cz);
                                            if (lData && lData->Get(cx, cy, cz) == 0)
                                            {
                                                lights[i].r = lData->GetRedLight(cx, cy, cz);
                                                lights[i].g = lData->GetGreenLight(cx, cy, cz);
                                                lights[i].b = lData->GetBlueLight(cx, cy, cz);
                                                lights[i].s = lData->GetSkyLight(cx, cy, cz);
                                                lights[i].use = true;
                                            }
                                            else
                                                lights[i].use = false;
                                        }
                                    }
                                }

                                // Create face
                                templight avg = templight::Avg(lights[3], lights[4], lights[6], lights[7]);
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 0, 1, 0, block.topTexMinX, block.topTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[4], lights[5], lights[7], lights[8]);
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, 0, 1, 0, block.topTexMaxX, block.topTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[0], lights[1], lights[3], lights[4]);
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 0, 1, 0, block.topTexMinX, block.topTexMaxY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[1], lights[2], lights[4], lights[5]);
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, 0, 1, 0, block.topTexMaxX, block.topTexMaxY, avg.GetPackedLightData() });
                            }
                            else
                            {
                                uint16_t lightData = nData ? nData->GetLightData(bx, by, bz) : 0;

                                // Up Face
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 1.0f, 0, 1, 0, block.topTexMinX, block.topTexMinY, lightData });
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 1.0f, 0, 1, 0, block.topTexMaxX, block.topTexMinY, lightData });
                                vertices.push_back({ x + 0.0f, y + 1.0f, z + 0.0f, 0, 1, 0, block.topTexMinX, block.topTexMaxY, lightData });
                                vertices.push_back({ x + 1.0f, y + 1.0f, z + 0.0f, 0, 1, 0, block.topTexMaxX, block.topTexMaxY, lightData });
                            }

                            AddIndices(indices, vertexCount);
                        }
                    }
                    // Down Face
                    {
                        int bx = x, by = y - 1, bz = z;
                        auto nData = GetChunk(m_neighboringChunkData, bx, by, bz);
                        bool down = nData ? nData->Get(bx, by, bz) == 0 : true;
                        if (down)
                        {
                            if (smoothLighting)
                            {
                                // Get 9 top blocks
                                std::array<templight, 9> lights;
                                {
                                    int ly = y - 1;
                                    int i = 0;
                                    for (int lz = z - 1; lz <= z + 1; lz++)
                                    {
                                        for (int lx = x - 1; lx <= x + 1; lx++, i++)
                                        {
                                            int cx = lx, cy = ly, cz = lz;
                                            auto lData = GetChunk(m_neighboringChunkData, cx, cy, cz);
                                            if (lData && lData->Get(cx, cy, cz) == 0)
                                            {
                                                lights[i].r = lData->GetRedLight(cx, cy, cz);
                                                lights[i].g = lData->GetGreenLight(cx, cy, cz);
                                                lights[i].b = lData->GetBlueLight(cx, cy, cz);
                                                lights[i].s = lData->GetSkyLight(cx, cy, cz);
                                                lights[i].use = true;
                                            }
                                            else
                                                lights[i].use = false;
                                        }
                                    }
                                }

                                // Create face
                                templight avg = templight::Avg(lights[4], lights[5], lights[7], lights[8]);
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, 0, -1, 0, block.bottomTexMinX, block.bottomTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[3], lights[4], lights[6], lights[7]);
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 0, -1, 0, block.bottomTexMaxX, block.bottomTexMinY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[1], lights[2], lights[4], lights[5]);
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, 0, -1, 0, block.bottomTexMinX, block.bottomTexMaxY, avg.GetPackedLightData() });

                                avg = templight::Avg(lights[0], lights[1], lights[3], lights[4]);
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 0, -1, 0, block.bottomTexMaxX, block.bottomTexMaxY, avg.GetPackedLightData() });
                            }
                            else
                            {
                                uint16_t lightData = nData ? nData->GetLightData(bx, by, bz) : 0;

                                // Down Face
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 1.0f, 0, -1, 0, block.bottomTexMinX, block.bottomTexMinY, lightData });
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 1.0f, 0, -1, 0, block.bottomTexMaxX, block.bottomTexMinY, lightData });
                                vertices.push_back({ x + 1.0f, y + 0.0f, z + 0.0f, 0, -1, 0, block.bottomTexMinX, block.bottomTexMaxY, lightData });
                                vertices.push_back({ x + 0.0f, y + 0.0f, z + 0.0f, 0, -1, 0, block.bottomTexMaxX, block.bottomTexMaxY, lightData });
                            }

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