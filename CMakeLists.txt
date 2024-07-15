cmake_minimum_required(VERSION 3.10)

# Project name
project(hypertube)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(LibtorrentRasterbar REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED CONFIG)

# Include directories
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${libtorrent_INCLUDE_DIRS})
include_directories(src/imgui)
include_directories(include include/app include/ui)

# Source files
file(GLOB IMGUI_SOURCES "src/app/imgui/*.cpp" "src/app/imgui/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${libtorrent_LIBRARIES} OpenGL::GL glfw)
