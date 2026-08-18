# Reusable Slint production libraries and the application executable.

add_library(hypertube_slint_models STATIC
    ${CMAKE_SOURCE_DIR}/src/ui/slint/SlintModelAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/slint/SearchModelAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/slint/DetailsModelAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/slint/LogModelAdapter.cpp
)
target_include_directories(hypertube_slint_models PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ui/slint
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(hypertube_slint_models PUBLIC Slint::Slint hypertube_slint_platform hypertube_utils)
slint_target_sources(hypertube_slint_models ${CMAKE_SOURCE_DIR}/ui/main-window.slint)

add_library(hypertube_slint_controller STATIC
    ${CMAKE_SOURCE_DIR}/src/ui/slint/SlintAppController.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/slint/SlintControllerFacades.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/slint/SlintRefreshCoordinators.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/slint/DialogService.cpp
)
target_include_directories(hypertube_slint_controller PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/app
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/ui/slint
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(hypertube_slint_controller PUBLIC
    hypertube_app hypertube_utils hypertube_torrent hypertube_config hypertube_search
    hypertube_presentation hypertube_slint_models Slint::Slint hypertube_slint_platform CURL::libcurl
)
add_executable(hypertube ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_include_directories(hypertube PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/app ${CMAKE_SOURCE_DIR}/include/utils ${CMAKE_SOURCE_DIR}/include/ui/slint)
target_link_libraries(hypertube PRIVATE hypertube_slint_controller)
if(WIN32)
    target_link_libraries(hypertube PRIVATE ole32)
endif()
hypertube_apply_release_options(hypertube)

if(WIN32 AND TARGET slint_cpp-shared)
    add_custom_command(TARGET hypertube POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:slint_cpp-shared>" "$<TARGET_FILE_DIR:hypertube>"
        VERBATIM
    )
endif()
