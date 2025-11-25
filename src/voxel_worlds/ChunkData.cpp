#include <wv/voxel_worlds/ChunkData.h>

namespace WillowVox
{
    void ChunkData::CalculateLighting()
    {
        struct LightEmission
        {
            int x;
            int y;
            int z;
            int lightLevel;
        };

        // Clear lighting data
        for (int i = 0; i < CHUNK_VOLUME; i++)
            lightLevels[i] = 0;

        std::queue<LightEmission> emissions;
        for (auto& emitter : lightEmitters)
        {
            int lightLevel = blockRegistry->GetBlock(Get(emitter.x, emitter.y, emitter.z)).lightLevel;
            emissions.push({ emitter.x, emitter.y, emitter.z, lightLevel });
        }

        while (!emissions.empty())
        {
            auto emission = emissions.front();
            emissions.pop();

            if (emission.lightLevel <= 1) continue;
            int nextLightLevel = emission.lightLevel - 1;

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
                if (!InBounds(b.x, b.y, b.z)) continue;
                if (GetLightLevel(b.x, b.y, b.z) >= nextLightLevel) continue;
                
                SetLightLevel(b.x, b.y, b.z, nextLightLevel);

                if (Get(b.x, b.y, b.z) == 0)
                {
                    emissions.push({ b.x, b.y, b.z, nextLightLevel });
                }
            }
        }
    }
}