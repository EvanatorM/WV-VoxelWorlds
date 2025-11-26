#include <wv/voxel_worlds/ChunkData.h>

#include <wv/voxel_worlds/ChunkManager.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace WillowVox
{
    void ChunkData::CalculateLighting(uint32_t currentVersion)
    {
        if (currentVersion == 0)
            currentVersion = m_version;
        if (currentVersion != m_version)
        {
            Logger::Log("Lighting calculation aborted for chunk (%d, %d, %d) due to version change.", id.x, id.y, id.z);
            return; // Abort lighting calculation if version has changed
        }

        struct LightEmission
        {
            int x;
            int y;
            int z;
            int lightLevel;
        };

        if (!surroundingDataCached)
        {
            // Cache surrounding chunks
            int index = 0;
            for (int z = -1; z <= 1; z++)
            {
                for (int y = -1; y <= 1; y++)
                {
                    for (int x = -1; x <= 1; x++)
                    {
                        if (x == 0 && y == 0 && z == 0) continue;
                        surroundingData[index++] = chunkManager->GetChunkData(id + glm::ivec3(x, y, z));
                    }
                }
            }
            surroundingDataCached = true;
        }

        // Clear lighting data
        for (int i = 0; i < CHUNK_VOLUME; i++)
            lightLevels[i] = 0;

        std::queue<LightEmission> emissions;
        for (auto& emitter : lightEmitters)
        {
            int lightLevel = blockRegistry->GetBlock(Get(emitter.x, emitter.y, emitter.z)).lightLevel;
            emissions.push({ emitter.x, emitter.y, emitter.z, lightLevel });
        }
        for (int c = 0; c < 26; c++)
        {
            if (!surroundingData[c]) continue;
            glm::ivec3 offset = (surroundingData[c]->id - id) * CHUNK_SIZE;
            for (auto& emitter : surroundingData[c]->lightEmitters)
            {
                glm::ivec3 emitterOffset = { offset.x + emitter.x, offset.y + emitter.y, offset.z + emitter.z };
                glm::ivec3 worldPos = id * CHUNK_SIZE;
                int lightLevel = blockRegistry->GetBlock(surroundingData[c]->Get(emitter.x, emitter.y, emitter.z)).lightLevel;
                /*Logger::Log("ID: %d %d %d / %d %d %d, Offset: %d %d %d, Emitter Offset: %d %d %d (Local: %d %d %d, World: %d %d %d). Calculated world pos: %d %d %d. Light Level: %d", id.x, id.y, id.z, 
                    surroundingData[c]->id.x, surroundingData[c]->id.y, surroundingData[c]->id.z, 
                    offset.x, offset.y, offset.z, 
                    emitterOffset.x, emitterOffset.y, emitterOffset.z,
                    emitter.x, emitter.y, emitter.z, 
                    emitter.x + offset.x, emitter.y + offset.y, emitter.z + offset.z,
                    emitterOffset.x + worldPos.x, emitterOffset.y + worldPos.y, emitterOffset.z + worldPos.z,
                    lightLevel
                );*/
                emissions.push({ emitterOffset.x, emitterOffset.y, emitterOffset.z, lightLevel });
            }
        }

        std::unordered_map<glm::ivec3, int> outOfBoundsLightLevels;

        while (!emissions.empty())
        {
            if (currentVersion != m_version)
            {
                Logger::Log("Lighting calculation aborted for chunk (%d, %d, %d) due to version change.", id.x, id.y, id.z);
                return; // Abort lighting calculation if version has changed
            }

            auto emission = emissions.front();
            emissions.pop();

            if (emission.lightLevel <= 1) continue;
            int nextLightLevel = emission.lightLevel - 1;
            //Logger::Log("Emission %d", nextLightLevel);

            std::vector<glm::ivec3> surroundingBlocks = {
                { emission.x + 1, emission.y, emission.z },
                { emission.x - 1, emission.y, emission.z },
                { emission.x, emission.y + 1, emission.z },
                { emission.x, emission.y - 1, emission.z },
                { emission.x, emission.y, emission.z + 1 },
                { emission.x, emission.y, emission.z - 1 },
            };

            for (auto& b : surroundingBlocks)
            {
                if (InBounds(b.x, b.y, b.z))
                {
                    if (GetLightLevel(b.x, b.y, b.z) >= nextLightLevel) continue;
                    SetLightLevel(b.x, b.y, b.z, nextLightLevel);

                    if (Get(b.x, b.y, b.z) == 0)
                    {
                        emissions.push({ b.x, b.y, b.z, nextLightLevel });
                    }
                }
                else
                {
                    int lightLevel;

                    auto it = outOfBoundsLightLevels.find(b);
                    if (it != outOfBoundsLightLevels.end())
                        lightLevel = outOfBoundsLightLevels[b];
                    else
                        lightLevel = 0;

                    if (lightLevel >= nextLightLevel) continue;
                    outOfBoundsLightLevels[b] = nextLightLevel;

                    emissions.push({ b.x, b.y, b.z, nextLightLevel });
                }
            }
        }

        Logger::Log("Finished lighting calculation for chunk (%d, %d, %d).", id.x, id.y, id.z);
    }
}