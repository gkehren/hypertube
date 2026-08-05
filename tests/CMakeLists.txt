include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
    test_string_utils.cpp
	test_torrent_add_flow.cpp
	test_system_utils.cpp
    test_presentation.cpp
)

target_include_directories(unit_tests PRIVATE
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/include/app"
	"${CMAKE_SOURCE_DIR}/include/ui"
)

add_executable(config_tests
    test_config_manager.cpp
)

add_executable(search_tests
	test_search_engine.cpp
)

add_executable(torrent_tests
	test_torrent_manager.cpp
)

add_executable(slint_model_tests
    test_slint_model_adapter.cpp
    "${CMAKE_SOURCE_DIR}/src/ui/slint/SlintModelAdapter.cpp"
    "${CMAKE_SOURCE_DIR}/src/ui/slint/SearchModelAdapter.cpp"
)

target_include_directories(slint_model_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/ui/slint"
    "${CMAKE_BINARY_DIR}"
)

target_link_libraries(slint_model_tests
    PRIVATE
    gtest_main
    Slint::Slint
)

if(TARGET PkgConfig::HYPERTUBE_FONTCONFIG)
    target_link_libraries(slint_model_tests PRIVATE PkgConfig::HYPERTUBE_FONTCONFIG)
endif()

add_dependencies(slint_model_tests hypertube-slint)

target_include_directories(config_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/app"
    "${json_SOURCE_DIR}/single_include/nlohmann"
)

target_include_directories(search_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/app"
    "${json_SOURCE_DIR}/single_include/nlohmann"
)

target_link_libraries(unit_tests
    PRIVATE
    gtest_main
    hypertube_utils
    hypertube_presentation
)

target_link_libraries(config_tests
    PRIVATE
    gtest_main
    hypertube_config
)

target_link_libraries(search_tests
    PRIVATE
    gtest_main
    hypertube_search
)

target_link_libraries(torrent_tests
	PRIVATE
	gtest_main
	hypertube_torrent
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(config_tests)
gtest_discover_tests(search_tests)
gtest_discover_tests(torrent_tests)
gtest_discover_tests(slint_model_tests)
