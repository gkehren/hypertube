cmake_minimum_required(VERSION 3.20)

# Set modern CMake policies to resolve warnings and use modern practices
cmake_policy(SET CMP0167 NEW) # Use Boost:: targets
cmake_policy(SET CMP0169 NEW) # Use FetchContent_MakeAvailable

# Project name
project(hypertube)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add compile definition for Windows to avoid SDK warnings
if(WIN32)
  add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

set(OpenGL_GL_PREFERENCE GLVND)

# Include FetchContent module for dependency management
include(FetchContent)

# Fetch imgui (docking branch)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# Fetch ImGuiFileDialog
FetchContent_Declare(
    ImGuiFileDialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG master
)
FetchContent_MakeAvailable(ImGuiFileDialog)

# Fetch nlohmann/json for JSON parsing
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(LibtorrentRasterbar CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED CONFIG)

# Explicitly list all source files
set(SOURCES
    src/main.cpp
    src/app/App.cpp
    src/app/ConfigManager.cpp
    src/app/TorrentManager.cpp
    src/ui/UIManager.cpp
    # Add core ImGui sources
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    # Add ImGui backend sources
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

# Create the executable
add_executable(hypertube ${SOURCES})

# Set target-specific include directories
target_include_directories(hypertube PRIVATE
    "include"
    "include/app"
    "include/ui"
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${json_SOURCE_DIR}/single_include/nlohmann"
)

# ImGuiFileDialog needs to find imgui.h, so we help it
target_include_directories(ImGuiFileDialog PUBLIC ${imgui_SOURCE_DIR})

# Link libraries using modern imported targets
target_link_libraries(hypertube PRIVATE
    Boost::filesystem
    Boost::system
    LibtorrentRasterbar::torrent-rasterbar
    OpenGL::GL
    glfw
    ImGuiFileDialog
    nlohmann_json::nlohmann_json
)

file(COPY config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Add optimized flags for Release build
if (MSVC)
  target_compile_options(hypertube PRIVATE
    $<$<CONFIG:Release>:/O2 /arch:AVX2 /GL /fp:fast>
  )
  target_link_options(hypertube PRIVATE
    $<$<CONFIG:Release>:/LTCG>
  )
else()
  target_compile_options(hypertube PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native -flto -ffast-math>
  )
  target_link_options(hypertube PRIVATE
    $<$<CONFIG:Release>:-flto>
  )
endif()