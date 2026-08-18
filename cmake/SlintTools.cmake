# Preview compilation and optional Slint visual tooling.

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
    ui/previews/toast-preview.slint
)
set(HYPERTUBE_SLINT_PREVIEW_OUTPUTS)
file(GLOB_RECURSE HYPERTUBE_SLINT_UI_DEPENDENCIES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/ui/*.slint)
foreach(preview_source IN LISTS HYPERTUBE_SLINT_PREVIEW_SOURCES)
    get_filename_component(preview_name ${preview_source} NAME_WE)
    set(preview_output ${CMAKE_CURRENT_BINARY_DIR}/${preview_name}.h)
    add_custom_command(
        OUTPUT ${preview_output}
        COMMAND $<TARGET_FILE:Slint::slint-compiler> -f cpp --style ${SLINT_STYLE}
            -I ${CMAKE_SOURCE_DIR}/ui -o ${preview_output} ${CMAKE_SOURCE_DIR}/${preview_source}
        DEPENDS ${HYPERTUBE_SLINT_UI_DEPENDENCIES}
        COMMENT "Checking Slint preview ${preview_name}"
        VERBATIM
    )
    list(APPEND HYPERTUBE_SLINT_PREVIEW_OUTPUTS ${preview_output})
endforeach()
add_custom_target(slint-preview-check DEPENDS ${HYPERTUBE_SLINT_PREVIEW_OUTPUTS})
add_test(NAME slint-preview-check COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target slint-preview-check --config $<CONFIG>)

add_executable(slint-visual-snapshots ${CMAKE_SOURCE_DIR}/tools/slint_visual_snapshots.cpp)
target_link_libraries(slint-visual-snapshots PRIVATE Slint::Slint hypertube_slint_platform)
slint_target_sources(slint-visual-snapshots ${CMAKE_SOURCE_DIR}/ui/snapshot-window.slint)

add_executable(slint-renderer-benchmark EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/slint_renderer_benchmark.cpp)
target_link_libraries(slint-renderer-benchmark PRIVATE Slint::Slint hypertube_slint_platform)
if(WIN32)
    target_link_libraries(slint-renderer-benchmark PRIVATE psapi)
    target_compile_options(slint-renderer-benchmark PRIVATE /EHsc)
endif()
slint_target_sources(slint-renderer-benchmark ${CMAKE_SOURCE_DIR}/ui/renderer-benchmark-window.slint)
if(WIN32 AND TARGET slint_cpp-shared)
    add_custom_command(TARGET slint-renderer-benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:slint_cpp-shared>" "$<TARGET_FILE_DIR:slint-renderer-benchmark>"
        VERBATIM
    )
endif()

set(HYPERTUBE_RENDERER_REPORT_DIRECTORY ${CMAKE_BINARY_DIR}/renderer-reports)
add_custom_target(slint-renderer-software-benchmark
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYPERTUBE_RENDERER_REPORT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E env SLINT_BACKEND=winit-software $<TARGET_FILE:slint-renderer-benchmark>
        ${HYPERTUBE_RENDERER_REPORT_DIRECTORY}/software.json software
    DEPENDS slint-renderer-benchmark USES_TERMINAL VERBATIM
)
if(HYPERTUBE_ENABLE_SLINT_GPU_BENCHMARK)
    add_custom_target(slint-renderer-comparison
        COMMAND ${CMAKE_COMMAND} -DBENCHMARK_EXECUTABLE=$<TARGET_FILE:slint-renderer-benchmark>
            -DOUTPUT_DIRECTORY=${HYPERTUBE_RENDERER_REPORT_DIRECTORY}
            -P ${CMAKE_SOURCE_DIR}/cmake/CompareSlintRenderers.cmake
        DEPENDS slint-renderer-benchmark USES_TERMINAL VERBATIM
    )
endif()
add_custom_target(slint-visual-snapshots-run
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/visual-artifacts
    COMMAND ${CMAKE_COMMAND} -E env SLINT_BACKEND=winit-software $<TARGET_FILE:slint-visual-snapshots> ${CMAKE_BINARY_DIR}/visual-artifacts
    DEPENDS slint-visual-snapshots USES_TERMINAL VERBATIM
)
