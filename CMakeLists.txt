cmake_minimum_required(VERSION 3.11)

# Project name
project(hypertube)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(OpenGL_GL_PREFERENCE GLVND)

# Include FetchContent module
include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Include directories
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${libtorrent_INCLUDE_DIRS})
include_directories(src/imgui)
include_directories(include include/app include/ui)
include_directories(${json_SOURCE_DIR}/single_include/nlohmann/)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(LibtorrentRasterbar CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED CONFIG)

# Source files
file(GLOB IMGUI_SOURCES "src/app/imgui/*.cpp")
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} PRIVATE LibtorrentRasterbar::torrent-rasterbar OpenGL::GL glfw)
