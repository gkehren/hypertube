if(NOT DEFINED BENCHMARK_EXECUTABLE OR NOT DEFINED OUTPUT_DIRECTORY)
    message(FATAL_ERROR "BENCHMARK_EXECUTABLE and OUTPUT_DIRECTORY are required")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
set(software_report "${OUTPUT_DIRECTORY}/software.json")
set(gpu_report "${OUTPUT_DIRECTORY}/femtovg.json")

foreach(renderer IN ITEMS software femtovg)
    if(renderer STREQUAL "software")
        set(backend "winit-software")
        set(report "${software_report}")
    else()
        set(backend "winit-femtovg")
        set(report "${gpu_report}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "SLINT_BACKEND=${backend}"
            "${BENCHMARK_EXECUTABLE}" "${report}" "${renderer}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    message(STATUS "${output}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${renderer} benchmark failed (${result}): ${error}")
    endif()
endforeach()

file(READ "${software_report}" software_json)
file(READ "${gpu_report}" gpu_json)
foreach(metric IN ITEMS completed_cycles wall_ms cpu_ms mean_cycle_ms p95_cycle_ms peak_rss_bytes stable)
    string(JSON software_${metric} GET "${software_json}" "${metric}")
    string(JSON gpu_${metric} GET "${gpu_json}" "${metric}")
endforeach()
if(NOT software_stable OR NOT gpu_stable)
    message(FATAL_ERROR "At least one renderer did not complete the stability run")
endif()

set(comparison_report "${OUTPUT_DIRECTORY}/comparison.json")
file(WRITE "${comparison_report}"
    "{\n  \"software\": ${software_json},\n  \"femtovg\": ${gpu_json}\n}\n")
message(STATUS "Renderer comparison written to ${comparison_report}")
message(STATUS "Software: mean=${software_mean_cycle_ms} ms, CPU=${software_cpu_ms} ms, RSS=${software_peak_rss_bytes} bytes")
message(STATUS "FemtoVG: mean=${gpu_mean_cycle_ms} ms, CPU=${gpu_cpu_ms} ms, RSS=${gpu_peak_rss_bytes} bytes")
