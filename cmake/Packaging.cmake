# Portable runtime installation and CPack packaging.

file(COPY ${CMAKE_SOURCE_DIR}/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
include(GNUInstallDirs)

if(WIN32)
    set(HYPERTUBE_RUNTIME_DEPENDENCY_DIRECTORY_ARGS)
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND HYPERTUBE_RUNTIME_DEPENDENCY_DIRECTORY_ARGS DIRECTORIES
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin)
    endif()
    install(TARGETS hypertube COMPONENT runtime RUNTIME_DEPENDENCY_SET hypertube_runtime_dependencies RUNTIME DESTINATION .)
    install(RUNTIME_DEPENDENCY_SET hypertube_runtime_dependencies COMPONENT runtime DESTINATION .
        ${HYPERTUBE_RUNTIME_DEPENDENCY_DIRECTORY_ARGS}
        PRE_EXCLUDE_REGEXES "api-ms-win-.*" "ext-ms-.*" "slint_cpp.*" "azureattest.*" "hvsifiletrust.*" "pdmutilities.*" "wpaxholder.*"
        POST_EXCLUDE_REGEXES ".*[\\/][Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\/].*")
    if(TARGET slint_cpp-shared)
        install(FILES "$<TARGET_FILE:slint_cpp-shared>" COMPONENT runtime DESTINATION .)
    endif()
else()
    install(TARGETS hypertube COMPONENT runtime RUNTIME DESTINATION . BUNDLE DESTINATION .)
endif()
install(DIRECTORY ${CMAKE_SOURCE_DIR}/config DESTINATION . COMPONENT runtime)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md ${CMAKE_SOURCE_DIR}/config/hypertube.desktop DESTINATION . COMPONENT runtime)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME Hypertube)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};runtime;.")
set(CPACK_MONOLITHIC_INSTALL ON)
include(CPack)
