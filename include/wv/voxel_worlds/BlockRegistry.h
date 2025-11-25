#pragma once

#include <wv/voxel_worlds/Block.h>
#include <wv/voxel_worlds/ChunkDefines.h>
#include <wv/core.h>
#include <stdexcept>

namespace WillowVox
{
    class BlockRegistry
    {
    public:
        static BlockRegistry& GetInstance()
        {
            static BlockRegistry instance;
            return instance;
        }

        void RegisterBlock(const std::string& strId, const std::string& texturePath,
            bool lightEmitter = false, int lightLevel = 16)
        {
            // Get texture id if exists or add to texture map if not
            int texId;
            auto it = m_tempTextures.find(texturePath);
            if (it == m_tempTextures.end())
            {
                m_tempTextures[texturePath] = ++m_tempTexCounter;
                texId = m_tempTexCounter;
            }
            else
                texId = it->second;

            // Set the temp block def texture to texture id
            m_tempBlockRegistry[strId] = { texId, texId, texId, lightEmitter, lightLevel };
        }

        void RegisterBlock(const std::string& strId, const std::string& topTexturePath,
            const std::string& bottomTexturePath,
            const std::string& sideTexturePath,
            bool lightEmitter = false, int lightLevel = 16)
        {
            // Get texture id if exists or add to texture map if not
            int topTexId, bottomTexId, sideTexId;
            {
                auto it = m_tempTextures.find(topTexturePath);
                if (it == m_tempTextures.end())
                {
                    m_tempTextures[topTexturePath] = ++m_tempTexCounter;
                    topTexId = m_tempTexCounter;
                }
                else
                    topTexId = it->second;
            }
            {
                auto it = m_tempTextures.find(bottomTexturePath);
                if (it == m_tempTextures.end())
                {
                    m_tempTextures[bottomTexturePath] = ++m_tempTexCounter;
                    bottomTexId = m_tempTexCounter;
                }
                else
                    bottomTexId = it->second;
            }
            {
                auto it = m_tempTextures.find(sideTexturePath);
                if (it == m_tempTextures.end())
                {
                    m_tempTextures[sideTexturePath] = ++m_tempTexCounter;
                    sideTexId = m_tempTexCounter;
                }
                else
                    sideTexId = it->second;
            }

            // Set the temp block def texture to texture id
            m_tempBlockRegistry[strId] = { topTexId, bottomTexId, sideTexId, lightEmitter, lightLevel };
        }

        void ApplyRegistry()
        {
            // Generate chunk texture
            // 1. Get texture count
            int numTextures = m_tempTextures.size();

            // 2. Determine size of chunk texture atlas
            int atlasWidth = 2;
            int atlasHeight = 2;
            while (atlasWidth * atlasHeight < numTextures)
            {
                //if (atlasWidth == atlasHeight)
                //    atlasWidth *= 2;
                //else
                //    atlasHeight *= 2;
                atlasWidth *= 2;
                atlasHeight *= 2;
            }

            // Get size of textures
            int texWidth, texHeight;
            for (auto& [path, id] : m_tempTextures)
            {
                auto texData = Texture::GetTextureData("assets/textures/blocks/" + path, texWidth, texHeight);
                break;
            }

            // 3. Create the texture
            int atlasPixelsX = atlasWidth * texWidth;
            int atlasPixelsY = atlasHeight * texHeight;
            std::vector<unsigned char> chunkTexture(
                (atlasWidth * texWidth * 4) * (atlasHeight * texHeight * 4));

            Logger::Log("Creating texture atlas with %d textures. Size: %dx%d (%dx%d pixels).", numTextures, atlasWidth, atlasHeight, atlasPixelsX, atlasPixelsY);

            // 4. Loop through textures and set the data in the texture atlas
            std::unordered_map<int, glm::vec4> texPositions;
            for (auto& [path, id] : m_tempTextures)
            {
                // Get texture data
                int width, height;
                auto texData = Texture::GetTextureData("assets/textures/blocks/" + path, width, height);

                // Validate texture size
                if (width != texWidth || height != texHeight)
                    Logger::Warn("Size of texture '%s' (%dx%d) does not match the expected size (%dx%d)", path, width, height, texWidth, texHeight);

                // Get start coordinates
                int xStart = (id % atlasWidth) * texWidth;
                int yStart = (id / atlasHeight) * texHeight;

                texPositions[id] = {
                    xStart / (float)atlasPixelsX,
                    yStart / (float)atlasPixelsY,
                    (xStart + texWidth) / (float)atlasPixelsX,
                    (yStart + texHeight) / (float)atlasPixelsY
                };

                // Set the texture data on the atlas
                for (int x = 0; x < texWidth; x++)
                {
                    for (int y = 0; y < texHeight; y++)
                    {
                        for (int c = 0; c < 4; c++)
                            chunkTexture[(y + yStart) * (atlasPixelsY * 4) + (x + xStart) * 4 + c] = texData[y * (texHeight * 4) + x * 4 + c];
                    }
                }
            }

            auto& am = AssetManager::GetInstance();
            auto tex = Texture::FromData(chunkTexture, atlasPixelsX, atlasPixelsY);
            am.AddAsset<Texture>("chunk_texture", tex);

            // Generate block definitions
            m_blocks[0] = { "air", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0 };
            for (auto& [strId, tex] : m_tempBlockRegistry)
            {
                auto& topPos = texPositions[tex.top];
                auto& bottomPos = texPositions[tex.bottom];
                auto& sidePos = texPositions[tex.side];
                Block block(strId,
                    topPos.x, topPos.z, topPos.y, topPos.w,
                    bottomPos.x, bottomPos.z, bottomPos.y, bottomPos.w,
                    sidePos.x, sidePos.z, sidePos.y, sidePos.w,
                    tex.lightEmitter, tex.lightLevel
                );
                m_blocks[++m_idCounter] = block;
                m_strIdToNumId[strId] = m_idCounter;
            }
        }

        const Block& GetBlock(const std::string& strId) const
        {
            auto it = m_strIdToNumId.find(strId);
            if (it == m_strIdToNumId.end())
            {
                Logger::Error("Invalid block id: %s", strId.c_str());
                throw std::out_of_range("Invalid block id");
            }
            else
                return m_blocks.at(it->second);
        }

        const Block& GetBlock(BlockId id) const
        {
            auto it = m_blocks.find(id);
            if (it == m_blocks.end())
            {
                Logger::Error("Invalid block id: %d", id);
                throw std::out_of_range("Invalid block ID");
            }
            else
                return it->second;
        }

    private:
        struct TempBlock
        {
            int top;
            int bottom;
            int side;
            bool lightEmitter;
            int lightLevel;
        };

        std::unordered_map<std::string, BlockId> m_strIdToNumId;
        std::unordered_map<BlockId, Block> m_blocks;
        BlockId m_idCounter = 0;

        std::unordered_map<std::string, int> m_tempTextures;
        int m_tempTexCounter = -1;
        std::unordered_map<std::string, TempBlock> m_tempBlockRegistry;
    };
}