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
            bool lightEmitter = false, int lightLevel = 16);
        void RegisterBlock(const std::string& strId, const std::string& topTexturePath,
            const std::string& bottomTexturePath,
            const std::string& sideTexturePath,
            bool lightEmitter = false, int lightLevel = 16);

        void ApplyRegistry();

        const Block& GetBlock(const std::string& strId) const;

        const Block& GetBlock(BlockId id) const;

        const BlockId GetBlockId(const std::string& strId) const;

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