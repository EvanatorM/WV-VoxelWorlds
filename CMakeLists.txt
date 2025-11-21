cmake_minimum_required(VERSION 3.24)
project(WVVoxelWorlds LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(WVVoxelWorlds STATIC
    src/voxel_worlds/ChunkManager.cpp
    src/voxel_worlds/ChunkRenderer.cpp
)

set_target_properties(WVVoxelWorlds PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_include_directories(WVVoxelWorlds PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>    
)

target_compile_features(WVVoxelWorlds PUBLIC cxx_std_20)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules.cmake OPTIONAL)

target_compile_definitions(WVVoxelWorlds PUBLIC
$<$<CONFIG:Debug>:DEBUG_MODE>
$<$<CONFIG:Release>:RELEASE_MODE>
)