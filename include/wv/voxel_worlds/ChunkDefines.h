#pragma once

#include <wv/wvpch.h>

constexpr int CHUNK_SIZE = 32;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
constexpr uint16_t CHUNK_DATA_VERSION = 1;
using BlockId = uint32_t;