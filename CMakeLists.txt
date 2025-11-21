cmake_minimum_required(VERSION 3.24)
project(WVVoxelWorlds LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(WVVoxelWorlds STATIC
    test.cpp
)

set_target_properties(WVVoxelWorlds PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_include_directories(WVVoxelWorlds PUBLIC include)

include(${CMAKE_SOURCE_DIR}/cmake/modules.cmake OPTIONAL)

target_compile_definitions(WVVoxelWorlds PUBLIC
$<$<CONFIG:Debug>:DEBUG_MODE>
$<$<CONFIG:Release>:RELEASE_MODE>
)