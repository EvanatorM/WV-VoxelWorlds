#pragma once

#include <wv/wvpch.h>

namespace WillowVox
{
    struct Block
    {
        Block() = default;
        Block(const std::string& strId, float texMinX, float texMaxX, float texMinY, float texMaxY, 
            bool lightEmitter = false, int lightLevel = 16)
            : strId(strId), topTexMinX(texMinX), topTexMaxX(texMaxX), topTexMinY(texMinY), topTexMaxY(texMaxY),
            bottomTexMinX(texMinX), bottomTexMaxX(texMaxX), bottomTexMinY(texMinY), bottomTexMaxY(texMaxY),
            sideTexMinX(texMinX), sideTexMaxX(texMaxX), sideTexMinY(texMinY), sideTexMaxY(texMaxY),
            lightEmitter(lightEmitter), lightLevel(lightLevel) {
        }
        

        Block(const std::string& strId, float topTexMinX, float topTexMaxX, float topTexMinY, float topTexMaxY,
            float bottomTexMinX, float bottomTexMaxX, float bottomTexMinY, float bottomTexMaxY,
            float sideTexMinX, float sideTexMaxX, float sideTexMinY, float sideTexMaxY,
            bool lightEmitter = false, int lightLevel = 16)
            : strId(strId), topTexMinX(topTexMinX), topTexMaxX(topTexMaxX), topTexMinY(topTexMinY), topTexMaxY(topTexMaxY),
            bottomTexMinX(bottomTexMinX), bottomTexMaxX(bottomTexMaxX), bottomTexMinY(bottomTexMinY), bottomTexMaxY(bottomTexMaxY),
            sideTexMinX(sideTexMinX), sideTexMaxX(sideTexMaxX), sideTexMinY(sideTexMinY), sideTexMaxY(sideTexMaxY),
            lightEmitter(lightEmitter), lightLevel(lightLevel) {
        }

        float topTexMinX, topTexMaxX, topTexMinY, topTexMaxY;
        float bottomTexMinX, bottomTexMaxX, bottomTexMinY, bottomTexMaxY;
        float sideTexMinX, sideTexMaxX, sideTexMinY, sideTexMaxY;
        std::string strId;

        bool lightEmitter;
        int lightLevel;
    };
}