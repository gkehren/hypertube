include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
    test_string_utils.cpp
)

add_executable(config_tests
    test_config_manager.cpp
)

add_executable(search_tests
    test_search_engine.cpp
)

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

include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(config_tests)
gtest_discover_tests(search_tests)
