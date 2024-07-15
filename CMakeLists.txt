cmake_minimum_required(VERSION 3.10)

# Project name
project(hypertube)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(LibtorrentRasterbar REQUIRED)
find_package(imgui REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED CONFIG)

# Include directories
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${libtorrent_INCLUDE_DIRS})
include_directories(${imgui_INCLUDE_DIRS})
include_directories(include include/app include/ui)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${libtorrent_LIBRARIES} PRIVATE imgui::imgui OpenGL::GL glfw)
