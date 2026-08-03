cmake_minimum_required(VERSION 3.20)

# Project name
project(hypertube CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(HYPERTUBE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Enable Position Independent Code (PIE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Support Homebrew package paths on macOS
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
    include_directories(SYSTEM "/opt/homebrew/include" "/usr/local/include")
    link_directories("/opt/homebrew/lib" "/usr/local/lib")
endif()

# Security Hardening Flags
if(MSVC)
    add_compile_options(/GS /sdl /guard:cf /utf-8)
    add_link_options(/DYNAMICBASE /NXCOMPAT /guard:cf)
else()
    # GCC/Clang
    add_compile_options(-fstack-protector-strong)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        if(NOT APPLE)
            add_compile_options(-fstack-clash-protection -fcf-protection)
        endif()
    endif()
    add_compile_options(-Wformat -Wformat-security)

    # Linker flags (RELRO, BIND_NOW)
    if(NOT APPLE)
        add_link_options("-Wl,-z,relro" "-Wl,-z,now")
    endif()

    # Fortify Source (Requires optimization, enabled for non-Debug builds)
    add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
endif()

# Add compile definition for Windows to avoid SDK warnings
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(HYPERTUBE_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(OpenGL_GL_PREFERENCE GLVND)

# Include FetchContent module for dependency management
include(FetchContent)

# 1. OpenGL dependency
find_package(OpenGL REQUIRED)

# 2. GLFW3 dependency (vcpkg / Homebrew / Linux or FetchContent fallback)
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND AND NOT TARGET glfw AND NOT TARGET glfw3::glfw)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GLFW3 QUIET glfw3)
    endif()
    if(GLFW3_FOUND)
        add_library(glfw INTERFACE IMPORTED)
        target_include_directories(glfw INTERFACE ${GLFW3_INCLUDE_DIRS})
        target_link_libraries(glfw INTERFACE ${GLFW3_LIBRARIES})
    else()
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
    endif()
endif()

if(TARGET glfw3::glfw AND NOT TARGET glfw)
    add_library(glfw ALIAS glfw3::glfw)
endif()

# 3. ImGui dependency (vcpkg / system or FetchContent fallback)
find_package(imgui CONFIG QUIET)
if(NOT imgui_FOUND AND NOT TARGET imgui AND NOT TARGET imgui::imgui)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
    add_library(imgui::imgui ALIAS imgui)
endif()

# 4. ImGuiFileDialog dependency (vcpkg / system or FetchContent fallback)
find_package(ImGuiFileDialog CONFIG QUIET)
if(NOT ImGuiFileDialog_FOUND AND NOT TARGET ImGuiFileDialog)
    FetchContent_Declare(
        ImGuiFileDialog
        GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(ImGuiFileDialog)

    if(TARGET ImGuiFileDialog AND TARGET imgui::imgui)
        target_link_libraries(ImGuiFileDialog PUBLIC imgui::imgui)
        if(imgui_SOURCE_DIR)
            target_include_directories(ImGuiFileDialog PUBLIC ${imgui_SOURCE_DIR})
        endif()
    endif()
endif()

# 5. nlohmann/json dependency (vcpkg / system or FetchContent fallback)
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND AND NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# 6. LibtorrentRasterbar dependency (vcpkg / Homebrew / Linux or FetchContent fallback)
find_package(LibtorrentRasterbar CONFIG QUIET)
if(NOT LibtorrentRasterbar_FOUND AND NOT TARGET LibtorrentRasterbar::torrent-rasterbar)
    find_package(libtorrent CONFIG QUIET)
endif()

if(NOT TARGET LibtorrentRasterbar::torrent-rasterbar AND NOT TARGET libtorrent::torrent-rasterbar)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBTORRENT QUIET libtorrent-rasterbar)
    endif()
    if(LIBTORRENT_FOUND)
        add_library(LibtorrentRasterbar::torrent-rasterbar INTERFACE IMPORTED)
        target_include_directories(LibtorrentRasterbar::torrent-rasterbar INTERFACE ${LIBTORRENT_INCLUDE_DIRS})
        target_link_libraries(LibtorrentRasterbar::torrent-rasterbar INTERFACE ${LIBTORRENT_LIBRARIES})
        target_compile_options(LibtorrentRasterbar::torrent-rasterbar INTERFACE ${LIBTORRENT_CFLAGS_OTHER})
    else()
        FetchContent_Declare(
            libtorrent
            GIT_REPOSITORY https://github.com/arvidn/libtorrent.git
            GIT_TAG v2.0.10
        )
        set(build_tests OFF CACHE BOOL "" FORCE)
        set(build_examples OFF CACHE BOOL "" FORCE)
        set(build_tools OFF CACHE BOOL "" FORCE)
        set(python-bindings OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(libtorrent)
        if(TARGET torrent-rasterbar AND NOT TARGET LibtorrentRasterbar::torrent-rasterbar)
            add_library(LibtorrentRasterbar::torrent-rasterbar ALIAS torrent-rasterbar)
        endif()
    endif()
endif()

if(TARGET libtorrent::torrent-rasterbar AND NOT TARGET LibtorrentRasterbar::torrent-rasterbar)
    add_library(LibtorrentRasterbar::torrent-rasterbar ALIAS libtorrent::torrent-rasterbar)
endif()

# 7. CURL dependency (vcpkg / Homebrew / Linux or FetchContent fallback)
find_package(CURL QUIET)
if(NOT CURL_FOUND AND NOT TARGET CURL::libcurl)
    FetchContent_Declare(
        curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG curl-8_5_0
    )
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)
    if(TARGET libcurl AND NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl ALIAS libcurl)
    endif()
else()
    if(NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl INTERFACE IMPORTED)
        target_include_directories(CURL::libcurl INTERFACE ${CURL_INCLUDE_DIRS})
        target_link_libraries(CURL::libcurl INTERFACE ${CURL_LIBRARIES})
    endif()
endif()

# 8. Find X11 on Linux systems if available
if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
endif()

# Source files
set(SOURCES
	src/main.cpp
	src/app/App.cpp
    src/ui/UIManager.cpp
    src/ui/TorrentTableUI.cpp
    src/ui/TorrentDetailsUI.cpp
    src/ui/SearchUI.cpp
    src/ui/ModalDialogs.cpp
    src/ui/LogsUI.cpp
    src/ui/Theme.cpp
)

# Create the utils library
add_library(hypertube_utils STATIC
    src/utils/AppPaths.cpp
    src/utils/Logger.cpp
    src/utils/StringUtils.cpp
    src/utils/SystemUtils.cpp
	src/utils/CredentialStore.cpp
)

target_include_directories(hypertube_utils PUBLIC
    "include"
    "include/utils"
)

target_link_libraries(hypertube_utils PUBLIC
    LibtorrentRasterbar::torrent-rasterbar
)

if(WIN32)
    target_link_libraries(hypertube_utils PUBLIC Advapi32)
elseif(APPLE)
    target_link_libraries(hypertube_utils PUBLIC "-framework Security")
endif()

# Create the config library for testing
add_library(hypertube_config STATIC
    src/app/ConfigManager.cpp
)

target_include_directories(hypertube_config PUBLIC
    "include"
    "include/app"
)

target_link_libraries(hypertube_config PUBLIC
    LibtorrentRasterbar::torrent-rasterbar
    hypertube_utils
    nlohmann_json::nlohmann_json
)

if(json_SOURCE_DIR)
    target_include_directories(hypertube_config PUBLIC
        "${json_SOURCE_DIR}/single_include/nlohmann"
        "${json_SOURCE_DIR}/single_include"
    )
endif()

# TorrentManager consumes the persisted TorrentConfigData model, so expose the
# configuration target explicitly instead of relying on executable include paths.
add_library(hypertube_torrent STATIC
	src/app/TorrentManager.cpp
)

target_include_directories(hypertube_torrent PUBLIC
	"include"
	"include/app"
	"include/utils"
)

target_link_libraries(hypertube_torrent PUBLIC
	hypertube_config
	hypertube_utils
	LibtorrentRasterbar::torrent-rasterbar
)

# Create the search library
add_library(hypertube_search STATIC
    src/app/SearchEngine.cpp
)

target_include_directories(hypertube_search PUBLIC
    "include"
    "include/app"
)

if(json_SOURCE_DIR)
    target_include_directories(hypertube_search PUBLIC
        "${json_SOURCE_DIR}/single_include/nlohmann"
        "${json_SOURCE_DIR}/single_include"
    )
endif()

target_link_libraries(hypertube_search PUBLIC
    hypertube_config
    hypertube_utils
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# Create the executable
add_executable(hypertube ${SOURCES})

# Set target-specific include directories
target_include_directories(hypertube PRIVATE
    "include"
    "include/app"
    "include/ui"
    "include/utils"
)

# Link libraries using modern imported targets
target_link_libraries(hypertube PRIVATE
	hypertube_utils
	hypertube_torrent
    hypertube_config
    hypertube_search
    LibtorrentRasterbar::torrent-rasterbar
    OpenGL::GL
    glfw
    imgui::imgui
    ImGuiFileDialog
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# Platform-specific libraries
if(UNIX AND NOT APPLE AND X11_FOUND)
    target_link_libraries(hypertube PRIVATE ${X11_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(hypertube PRIVATE ws2_32 iphlpapi crypt32 dbghelp)
endif()

file(COPY config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Portable distribution: install the executable and seed configuration into a
# flat directory that can be zipped for Linux/macOS/Windows packaging jobs.
include(GNUInstallDirs)
install(TARGETS hypertube COMPONENT runtime RUNTIME DESTINATION . BUNDLE DESTINATION .)
install(DIRECTORY config DESTINATION . COMPONENT runtime)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "Hypertube")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};runtime;.")
set(CPACK_MONOLITHIC_INSTALL ON)
include(CPack)

# Add optimized flags for Release build
if(MSVC)
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

enable_testing()
add_subdirectory(tests)
