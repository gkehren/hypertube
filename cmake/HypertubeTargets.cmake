# Domain and toolkit-neutral production libraries.

add_library(hypertube_utils STATIC
    ${CMAKE_SOURCE_DIR}/src/utils/AppPaths.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/StringUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SystemUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/CredentialStore.cpp
)
target_include_directories(hypertube_utils PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/utils)
target_link_libraries(hypertube_utils PUBLIC LibtorrentRasterbar::torrent-rasterbar)
if(WIN32)
    target_link_libraries(hypertube_utils PUBLIC Advapi32)
elseif(APPLE)
    target_link_libraries(hypertube_utils PUBLIC "-framework Security" "-framework CoreFoundation")
endif()

add_library(hypertube_config STATIC ${CMAKE_SOURCE_DIR}/src/app/ConfigManager.cpp)
target_include_directories(hypertube_config PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/app)
target_link_libraries(hypertube_config PUBLIC LibtorrentRasterbar::torrent-rasterbar hypertube_utils nlohmann_json::nlohmann_json)
if(json_SOURCE_DIR)
    target_include_directories(hypertube_config PUBLIC ${json_SOURCE_DIR}/single_include/nlohmann ${json_SOURCE_DIR}/single_include)
endif()

add_library(hypertube_torrent STATIC ${CMAKE_SOURCE_DIR}/src/app/TorrentManager.cpp)
target_include_directories(hypertube_torrent PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/app ${CMAKE_SOURCE_DIR}/include/utils)
target_link_libraries(hypertube_torrent PUBLIC hypertube_config hypertube_utils LibtorrentRasterbar::torrent-rasterbar)

add_library(hypertube_search STATIC ${CMAKE_SOURCE_DIR}/src/app/SearchEngine.cpp)
target_include_directories(hypertube_search PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/app)
if(json_SOURCE_DIR)
    target_include_directories(hypertube_search PUBLIC ${json_SOURCE_DIR}/single_include/nlohmann ${json_SOURCE_DIR}/single_include)
endif()
target_link_libraries(hypertube_search PUBLIC hypertube_config hypertube_utils nlohmann_json::nlohmann_json CURL::libcurl pugixml::pugixml)

add_library(hypertube_presentation STATIC
    ${CMAKE_SOURCE_DIR}/src/presentation/UiFormatters.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/TorrentListPresenter.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/TorrentAvailability.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/TorrentDetailsPresenter.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/SearchPresenter.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/LogsPresenter.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/PreferencesController.cpp
    ${CMAKE_SOURCE_DIR}/src/presentation/UiStateController.cpp
)
target_include_directories(hypertube_presentation PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/app ${CMAKE_SOURCE_DIR}/include/utils)
target_link_libraries(hypertube_presentation PUBLIC hypertube_torrent hypertube_search hypertube_utils LibtorrentRasterbar::torrent-rasterbar)

add_library(hypertube_app STATIC ${CMAKE_SOURCE_DIR}/src/app/App.cpp)
target_include_directories(hypertube_app PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/app ${CMAKE_SOURCE_DIR}/include/utils)
target_link_libraries(hypertube_app PUBLIC hypertube_torrent hypertube_search hypertube_config hypertube_utils CURL::libcurl)

add_executable(torrent-presentation-benchmark EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tests/benchmark_torrent_presentation.cpp)
target_include_directories(torrent-presentation-benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include)
