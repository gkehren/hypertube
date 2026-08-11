# Project-wide compiler and linker policy.

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/GS /sdl /guard:cf /utf-8)
    add_link_options(/DYNAMICBASE /NXCOMPAT /guard:cf)
else()
    add_compile_options(-fstack-protector-strong)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64" AND NOT APPLE)
        add_compile_options(-fstack-clash-protection -fcf-protection)
    endif()
    add_compile_options(-Wformat -Wformat-security)

    if(NOT APPLE)
        add_link_options("-Wl,-z,relro" "-Wl,-z,now")
    endif()

    add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
endif()

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(HYPERTUBE_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

function(hypertube_apply_release_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:/O2 /GL>
            $<$<AND:$<CONFIG:Release>,$<BOOL:${HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS}>>:/arch:AVX2 /fp:fast>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/LTCG>
        )
    else()
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:-O3 -flto>
            $<$<AND:$<CONFIG:Release>,$<BOOL:${HYPERTUBE_ENABLE_NATIVE_OPTIMIZATIONS}>>:-march=native -ffast-math>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:-flto>
        )
    endif()
endfunction()
