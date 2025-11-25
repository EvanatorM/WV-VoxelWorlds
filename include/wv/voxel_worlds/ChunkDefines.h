#pragma once

#include <wv/wvpch.h>

constexpr int CHUNK_SIZE = 32;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
using BlockId = uint32_t;