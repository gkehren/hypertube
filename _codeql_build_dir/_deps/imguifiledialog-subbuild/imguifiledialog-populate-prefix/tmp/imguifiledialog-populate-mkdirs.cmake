# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-src")
  file(MAKE_DIRECTORY "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-src")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-build"
  "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix"
  "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/tmp"
  "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp"
  "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src"
  "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/hypertube/hypertube/_codeql_build_dir/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
