cmake_minimum_required(VERSION 3.11)

# Project name
project(hypertube)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(OpenGL_GL_PREFERENCE GLVND)

# Include FetchContent module
include(FetchContent)

# Fetch imgui docking branch
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
	ImGuiFileDialog
	GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
	GIT_TAG master
)
FetchContent_GetProperties(ImGuiFileDialog)
if (NOT ImGuiFileDialog_POPULATED)
	FetchContent_Populate(ImGuiFileDialog)
	add_subdirectory(${ImGuiFileDialog_SOURCE_DIR} ${ImGuiFileDialog_BINARY_DIR})
endif()

# Fetch dirent for Windows
if (WIN32)
	FetchContent_Declare(
		dirent
		GIT_REPOSITORY https://github.com/tronkko/dirent.git
		GIT_TAG 1.24
	)
	FetchContent_MakeAvailable(dirent)
	include_directories(${dirent_SOURCE_DIR}/include)
endif()

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
include_directories(${imgui_SOURCE_DIR})
include_directories(${imgui_SOURCE_DIR}/backends)
include_directories(${json_SOURCE_DIR}/single_include/nlohmann/)
include_directories(include include/app include/ui)

target_include_directories(ImGuiFileDialog PUBLIC ${imgui_SOURCE_DIR})

# Find required packages
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(LibtorrentRasterbar CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED CONFIG)

# ImGui Sources
set(IMGUI_SOURCES
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
	${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_SOURCE_DIR}/config
			$<TARGET_FILE_DIR:${PROJECT_NAME}>/config
	COMMENT "Copying config directory to build directory"
)

# Link libraries
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} PRIVATE LibtorrentRasterbar::torrent-rasterbar OpenGL::GL glfw ImGuiFileDialog)
