cmake_minimum_required(VERSION 3.21)

project(hypertube VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HYPERTUBE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS
    "Tune Release binaries for the build host (not suitable for distributed packages)" OFF)
option(HYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK
    "Build the optional FemtoVG renderer and renderer comparison target" OFF)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerOptions.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake)

add_compile_definitions(HYPERTUBE_VERSION="${PROJECT_VERSION}")

include(${CMAKE_SOURCE_DIR}/cmake/HypertubeTargets.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/SlintFrontend.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/SlintTools.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake)

enable_testing()
add_subdirectory(tests)
