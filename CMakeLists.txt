cmake_minimum_required(VERSION 3.21)

# Project name
project(hypertube CXX)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HYPERTUBE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS
    "Tune Release binaries for the build host (not suitable for distributed packages)" OFF)
option(HYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK
    "Build the optional FemtoVG renderer and renderer comparison target" OFF)

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

# Include FetchContent module for dependency management
include(FetchContent)

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

# 8. Slint dependency. Keep the version and backend choices explicit so a
# local Qt installation cannot silently change the renderer.
set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG ${HYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK} CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG v1.16.1
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(HYPERTUBE_FONTCONFIG QUIET IMPORTED_TARGET fontconfig)
    endif()
    if(NOT TARGET PkgConfig::HYPERTUBE_FONTCONFIG)
        message(FATAL_ERROR "Fontconfig development files are required for Slint")
    endif()
endif()

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
    target_link_libraries(hypertube_utils PUBLIC "-framework Security" "-framework CoreFoundation")
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

# Toolkit-neutral presentation and controller layer.
add_library(hypertube_presentation STATIC
    src/presentation/UiFormatters.cpp
  src/presentation/TorrentListPresenter.cpp
	  src/presentation/TorrentAvailability.cpp
    src/presentation/TorrentDetailsPresenter.cpp
    src/presentation/SearchPresenter.cpp
    src/presentation/LogsPresenter.cpp
    src/presentation/PreferencesController.cpp
    src/presentation/UiStateController.cpp
)

target_include_directories(hypertube_presentation PUBLIC
    "include"
    "include/app"
    "include/utils"
)

target_link_libraries(hypertube_presentation PUBLIC
    hypertube_torrent
    hypertube_search
    hypertube_utils
    LibtorrentRasterbar::torrent-rasterbar
)

if(NOT SLINT_STYLE)
    set(SLINT_STYLE "fluent-dark" CACHE STRING "Slint widget style used by the application and previews" FORCE)
endif()

add_executable(hypertube
        src/main.cpp
        src/app/App.cpp
        src/ui/slint/SlintAppController.cpp
        src/ui/slint/SlintControllerFacades.cpp
		src/ui/slint/SlintRefreshCoordinators.cpp
        src/ui/slint/SlintModelAdapter.cpp
        src/ui/slint/SearchModelAdapter.cpp
        src/ui/slint/DetailsModelAdapter.cpp
        src/ui/slint/LogModelAdapter.cpp
        src/ui/slint/DialogService.cpp
    )

    target_include_directories(hypertube PRIVATE
        "include"
        "include/app"
        "include/utils"
        "include/ui/slint"
    )

    target_link_libraries(hypertube PRIVATE
        hypertube_utils
        hypertube_torrent
        hypertube_config
        hypertube_search
        hypertube_presentation
        Slint::Slint
        CURL::libcurl
    )

    if(TARGET PkgConfig::HYPERTUBE_FONTCONFIG)
        target_link_libraries(hypertube PRIVATE PkgConfig::HYPERTUBE_FONTCONFIG)
    endif()

    if(WIN32)
        target_link_libraries(hypertube PRIVATE ole32)
    endif()

    slint_target_sources(hypertube
        ui/main-window.slint
    )

    if(WIN32 AND TARGET slint_cpp-shared)
            add_custom_command(TARGET hypertube POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:slint_cpp-shared>"
                "$<TARGET_FILE_DIR:hypertube>"
            VERBATIM
        )
    endif()

# Keep the static layout previews independently checkable without making them
# part of the production executable or requiring slint-viewer at runtime.
set(HYPERTUBE_SLINT_PREVIEW_SOURCES
    ui/previews/torrent-table-preview.slint
    ui/previews/search-preview.slint
    ui/previews/preferences-preview.slint
    ui/previews/details-preview.slint
    ui/previews/favorites-preview.slint
    ui/previews/logs-preview.slint
    ui/previews/add-torrent-dialog-preview.slint
    ui/previews/remove-torrent-dialog-preview.slint
    ui/previews/app-shell-preview.slint
)
set(HYPERTUBE_SLINT_PREVIEW_OUTPUTS)
file(GLOB_RECURSE HYPERTUBE_SLINT_UI_DEPENDENCIES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/ui/*.slint"
)
foreach(preview_source IN LISTS HYPERTUBE_SLINT_PREVIEW_SOURCES)
    get_filename_component(preview_name "${preview_source}" NAME_WE)
    set(preview_output "${CMAKE_CURRENT_BINARY_DIR}/${preview_name}.h")
    add_custom_command(
        OUTPUT "${preview_output}"
        COMMAND $<TARGET_FILE:Slint::slint-compiler>
            -f cpp
            --style "${SLINT_STYLE}"
            -I "${CMAKE_SOURCE_DIR}/ui"
            -o "${preview_output}"
            "${CMAKE_SOURCE_DIR}/${preview_source}"
        DEPENDS
            ${HYPERTUBE_SLINT_UI_DEPENDENCIES}
        COMMENT "Checking Slint preview ${preview_name}"
        VERBATIM
    )
    list(APPEND HYPERTUBE_SLINT_PREVIEW_OUTPUTS "${preview_output}")
endforeach()
add_custom_target(slint-preview-check DEPENDS ${HYPERTUBE_SLINT_PREVIEW_OUTPUTS})
add_test(
    NAME slint-preview-check
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target slint-preview-check --config $<CONFIG>
)

add_executable(slint-visual-snapshots tools/slint_visual_snapshots.cpp)
target_link_libraries(slint-visual-snapshots PRIVATE Slint::Slint)
slint_target_sources(slint-visual-snapshots ui/snapshot-window.slint)

add_executable(slint-renderer-benchmark EXCLUDE_FROM_ALL tools/slint_renderer_benchmark.cpp)
target_link_libraries(slint-renderer-benchmark PRIVATE Slint::Slint)
if(WIN32)
    target_link_libraries(slint-renderer-benchmark PRIVATE psapi)
    target_compile_options(slint-renderer-benchmark PRIVATE /EHsc)
endif()
slint_target_sources(slint-renderer-benchmark ui/renderer-benchmark-window.slint)
if(WIN32 AND TARGET slint_cpp-shared)
    add_custom_command(TARGET slint-renderer-benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:slint_cpp-shared>"
            "$<TARGET_FILE_DIR:slint-renderer-benchmark>"
        VERBATIM
    )
endif()

set(HYPERTUBE_RENDERER_REPORT_DIRECTORY "${CMAKE_BINARY_DIR}/renderer-reports")
add_custom_target(slint-renderer-software-benchmark
    COMMAND ${CMAKE_COMMAND} -E make_directory "${HYPERTUBE_RENDERER_REPORT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E env SLINT_BACKEND=winit-software
        $<TARGET_FILE:slint-renderer-benchmark>
        "${HYPERTUBE_RENDERER_REPORT_DIRECTORY}/software.json" software
    DEPENDS slint-renderer-benchmark
    USES_TERMINAL
    VERBATIM
)
if(HYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK)
    add_custom_target(slint-renderer-comparison
        COMMAND ${CMAKE_COMMAND}
            -DBENCHMARK_EXECUTABLE=$<TARGET_FILE:slint-renderer-benchmark>
            -DOUTPUT_DIRECTORY=${HYPERTUBE_RENDERER_REPORT_DIRECTORY}
            -P "${CMAKE_SOURCE_DIR}/cmake/CompareSlintRenderers.cmake"
        DEPENDS slint-renderer-benchmark
        USES_TERMINAL
        VERBATIM
    )
endif()
add_custom_target(slint-visual-snapshots-run
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/visual-artifacts"
    COMMAND ${CMAKE_COMMAND} -E env SLINT_BACKEND=winit-software
        $<TARGET_FILE:slint-visual-snapshots> "${CMAKE_BINARY_DIR}/visual-artifacts"
    DEPENDS slint-visual-snapshots
    USES_TERMINAL
)

file(COPY config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Portable distribution: install the executable and seed configuration into a
# flat directory that can be zipped for Linux/macOS/Windows packaging jobs.
include(GNUInstallDirs)
if(WIN32)
    set(HYPERTUBE_RUNTIME_DEPENDENCY_DIRECTORY_ARGS)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND HYPERTUBE_RUNTIME_DEPENDENCY_DIRECTORY_ARGS
            DIRECTORIES
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
        )
    endif()

    install(TARGETS hypertube
        COMPONENT runtime
        RUNTIME_DEPENDENCY_SET hypertube_runtime_dependencies
        RUNTIME DESTINATION .
    )
    install(RUNTIME_DEPENDENCY_SET hypertube_runtime_dependencies
        COMPONENT runtime
        DESTINATION .
        ${HYPERTUBE_RUNTIME_DEPENDENCY_DIRECTORY_ARGS}
        PRE_EXCLUDE_REGEXES
            "api-ms-win-.*"
            "ext-ms-.*"
            "slint_cpp.*"
            "azureattest.*"
            "hvsifiletrust.*"
            "pdmutilities.*"
            "wpaxholder.*"
        POST_EXCLUDE_REGEXES
            ".*[\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\/].*"
    )
    if(TARGET slint_cpp-shared)
        install(FILES "$<TARGET_FILE:slint_cpp-shared>" COMPONENT runtime DESTINATION .)
    endif()
else()
    install(TARGETS hypertube COMPONENT runtime RUNTIME DESTINATION . BUNDLE DESTINATION .)
endif()
install(DIRECTORY config DESTINATION . COMPONENT runtime)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "Hypertube")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};runtime;.")
set(CPACK_MONOLITHIC_INSTALL ON)
include(CPack)

if(MSVC)
    target_compile_options(hypertube PRIVATE
        $<$<CONFIG:Release>:/O2 /GL /fp:fast>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS}>>:/arch:AVX2>
    )
    target_link_options(hypertube PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
else()
    target_compile_options(hypertube PRIVATE
        $<$<CONFIG:Release>:-O3 -flto -ffast-math>
        $<$<AND:$<CONFIG:Release>,$<BOOL:${HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS}>>:-march=native>
    )
    target_link_options(hypertube PRIVATE
        $<$<CONFIG:Release>:-flto>
    )
endif()

enable_testing()
add_subdirectory(tests)
