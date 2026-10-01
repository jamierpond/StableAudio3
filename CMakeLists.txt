cmake_minimum_required(VERSION 3.31)

if (APPLE)
    project(StableAudio3 VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else ()
    project(StableAudio3 VERSION 0.1.0 LANGUAGES C CXX)
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

find_package(eacp REQUIRED)
eacp_default_setup()

enable_testing()

add_subdirectory(TextEncoder)
add_subdirectory(Codec)
add_subdirectory(DiT)
add_subdirectory(Sampler)
add_subdirectory(Tests)

add_library(eacp-sa3-checkpoints STATIC Checkpoints.cpp)
target_include_directories(eacp-sa3-checkpoints PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(eacp-sa3-checkpoints PUBLIC eacp-core PRIVATE eacp-network)
set_default_target_setting(eacp-sa3-checkpoints)

add_executable(StableAudio3 Main.cpp)

target_link_libraries(StableAudio3 PRIVATE
        eacp-sa3-checkpoints
        eacp-sa3-text-encoder
        eacp-sa3-dit
        eacp-sa3-codec
        eacp-sa3-sampler)

set_default_target_setting(StableAudio3)
