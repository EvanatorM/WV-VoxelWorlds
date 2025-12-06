#pragma once

#include <wv/wvpch.h>
#include <wv/voxel_worlds/ChunkDefines.h>

namespace WillowVox
{
    struct Block
    {
        Block() = default;
        Block(const std::string& strId, BlockId id, float texMinX, float texMaxX, float texMinY, float texMaxY, 
            bool lightEmitter = false, int redLight = 15, int greenLight = 15, int blueLight = 15)
            : strId(strId), id(id), topTexMinX(texMinX), topTexMaxX(texMaxX), topTexMinY(texMinY), topTexMaxY(texMaxY),
            bottomTexMinX(texMinX), bottomTexMaxX(texMaxX), bottomTexMinY(texMinY), bottomTexMaxY(texMaxY),
            sideTexMinX(texMinX), sideTexMaxX(texMaxX), sideTexMinY(texMinY), sideTexMaxY(texMaxY),
            lightEmitter(lightEmitter), redLight(redLight), greenLight(greenLight), blueLight(blueLight) {
        }
        

        Block(const std::string& strId, BlockId id, float topTexMinX, float topTexMaxX, float topTexMinY, float topTexMaxY,
            float bottomTexMinX, float bottomTexMaxX, float bottomTexMinY, float bottomTexMaxY,
            float sideTexMinX, float sideTexMaxX, float sideTexMinY, float sideTexMaxY,
            bool lightEmitter = false, int redLight = 15, int greenLight = 15, int blueLight = 15)
            : strId(strId), id(id), topTexMinX(topTexMinX), topTexMaxX(topTexMaxX), topTexMinY(topTexMinY), topTexMaxY(topTexMaxY),
            bottomTexMinX(bottomTexMinX), bottomTexMaxX(bottomTexMaxX), bottomTexMinY(bottomTexMinY), bottomTexMaxY(bottomTexMaxY),
            sideTexMinX(sideTexMinX), sideTexMaxX(sideTexMaxX), sideTexMinY(sideTexMinY), sideTexMaxY(sideTexMaxY),
            lightEmitter(lightEmitter), redLight(redLight), greenLight(greenLight), blueLight(blueLight) {
        }

        float topTexMinX, topTexMaxX, topTexMinY, topTexMaxY;
        float bottomTexMinX, bottomTexMaxX, bottomTexMinY, bottomTexMaxY;
        float sideTexMinX, sideTexMaxX, sideTexMinY, sideTexMaxY;
        std::string strId;
        BlockId id;

        bool lightEmitter;
        int redLight, greenLight, blueLight;
    };
}