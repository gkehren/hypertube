# Third-party dependencies and platform integration required by Hypertube.

include(FetchContent)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
    include_directories(SYSTEM "/opt/homebrew/include" "/usr/local/include")
    link_directories("/opt/homebrew/lib" "/usr/local/lib")
endif()

if(NOT DEFINED SLINT_STYLE OR SLINT_STYLE STREQUAL "")
    set(
        SLINT_STYLE "fluent-dark"
        CACHE STRING
        "Slint widget style used by the application and previews"
    )
endif()

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND AND NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

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

find_package(pugixml QUIET)
if(NOT pugixml_FOUND AND NOT TARGET pugixml::pugixml AND NOT TARGET pugixml)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.14
    )
    FetchContent_MakeAvailable(pugixml)
    if(TARGET pugixml AND NOT TARGET pugixml::pugixml)
        add_library(pugixml::pugixml ALIAS pugixml)
    endif()
endif()

set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG ${HYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK} CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG v1.17.1
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

add_library(hypertube_slint_platform INTERFACE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(HYPERTUBE_FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    target_link_libraries(hypertube_slint_platform INTERFACE PkgConfig::HYPERTUBE_FONTCONFIG)
elseif(APPLE)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(CORETEXT_FRAMEWORK CoreText REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(CORESERVICES_FRAMEWORK CoreServices REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    target_link_libraries(hypertube_slint_platform INTERFACE
        ${APPKIT_FRAMEWORK} ${CARBON_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK}
        ${CORETEXT_FRAMEWORK} ${COREVIDEO_FRAMEWORK} ${CORESERVICES_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK} ${QUARTZCORE_FRAMEWORK}
    )
elseif(WIN32)
    target_link_libraries(hypertube_slint_platform INTERFACE imm32)
endif()
