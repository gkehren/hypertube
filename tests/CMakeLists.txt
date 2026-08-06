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
    test_preferences_controller.cpp
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

if(HYPERTUBE_BUILD_SLINT)
    add_executable(slint_model_tests
        test_slint_model_adapter.cpp
        "${CMAKE_SOURCE_DIR}/src/ui/slint/SlintModelAdapter.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/slint/SearchModelAdapter.cpp"
        "${CMAKE_SOURCE_DIR}/src/ui/slint/DetailsModelAdapter.cpp"
    )

    target_include_directories(slint_model_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/include/ui/slint"
        "${CMAKE_BINARY_DIR}"
    )

    target_link_libraries(slint_model_tests
        PRIVATE
        gtest_main
        hypertube_utils
        Slint::Slint
    )

    if(TARGET PkgConfig::HYPERTUBE_FONTCONFIG)
        target_link_libraries(slint_model_tests PRIVATE PkgConfig::HYPERTUBE_FONTCONFIG)
    endif()

    add_dependencies(slint_model_tests hypertube-slint)
endif()

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

if(WIN32)
	# Multi-config generators place GoogleTest and Slint DLLs in their own
	# runtime directories.  gtest_discover_tests() launches each executable
	# during the build, before CTest can provide a custom environment, so make
	# the runtime dependencies available beside the test binaries.
	function(copy_windows_test_runtime test_target)
		foreach(runtime_target IN ITEMS gtest gtest_main)
			if(TARGET ${runtime_target})
				add_custom_command(TARGET ${test_target} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different
						"$<TARGET_FILE:${runtime_target}>"
						"$<TARGET_FILE_DIR:${test_target}>"
					VERBATIM
				)
			endif()
		endforeach()
	endfunction()

	copy_windows_test_runtime(unit_tests)
	copy_windows_test_runtime(config_tests)
	copy_windows_test_runtime(search_tests)
	copy_windows_test_runtime(torrent_tests)
    if(HYPERTUBE_BUILD_SLINT)
        copy_windows_test_runtime(slint_model_tests)
        if(TARGET slint_cpp-shared)
            add_custom_command(TARGET slint_model_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:slint_cpp-shared>"
                    "$<TARGET_FILE_DIR:slint_model_tests>"
                VERBATIM
            )
        endif()
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(config_tests)
gtest_discover_tests(search_tests)
gtest_discover_tests(torrent_tests)
if(HYPERTUBE_BUILD_SLINT)
    gtest_discover_tests(slint_model_tests)
endif()
