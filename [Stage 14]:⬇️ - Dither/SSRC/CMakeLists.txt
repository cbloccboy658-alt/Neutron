option(BUILD_TESTS "Build tests" ON)
option(BUILD_CLI "Build utilities" ON)
option(ENABLE_ASAN "Enable ASAN" OFF)

# To build with "-march=native" option
# CC=gcc-12 CXX=g++-12 CFLAGS="-march=native" CXXFLAGS="-march=native" cmake ..

# To use clang-19 to Build with LTO enabled
# CXX=clang++-19 CC=clang-19 cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=True -DCMAKE_CXX_COMPILER_AR=/usr/bin/llvm-ar-19 -DCMAKE_CXX_COMPILER_RANLIB=/usr/bin/llvm-ranlib-19

# To build shared library
# cmake .. -DBUILD_SHARED_LIBS=True -DCMAKE_POSITION_INDEPENDENT_CODE=ON

##

set(SSRC_VERSION 2.4.2)

message(STATUS "Configuring Shibatch Sample Rate Converter ${SSRC_VERSION}")

cmake_minimum_required(VERSION 3.18)
project(SSRC VERSION ${SSRC_VERSION} LANGUAGES C CXX)

add_compile_definitions(SSRC_VERSION="${SSRC_VERSION}")
set(SSRC_SOVERSION ${SSRC_VERSION_MAJOR})

#

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)

# CMake configuration

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source build not supported")
endif()

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

include_directories("${PROJECT_SOURCE_DIR}/src/include")

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

string(TIMESTAMP BUILDDATE "%Y-%m-%dT%H:%M:%SZ" UTC)
add_compile_definitions(BUILDINFO="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${BUILDDATE} ${CMAKE_BUILD_TYPE}")

include(GNUInstallDirs)

include(CTest)

# Compiler options

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fcompare-debug-second")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -Og")
  if (ENABLE_ASAN)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(arm64|aarch64)")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=hwaddress -static-libasan")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=hwaddress -static-libasan")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -static-libasan")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -static-libasan")
    endif()
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-math-errno")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -Og")
  if (ENABLE_ASAN)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(arm64|aarch64)")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=hwaddress")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=hwaddress")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    endif()
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
  get_filename_component(COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME_WE)
  string(TOLOWER "${COMPILER_BASENAME}" LC_COMPILER_BASENAME)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" LC_CMAKE_BUILD_TYPE)

if (WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS=1 _CRT_NONSTDC_NO_DEPRECATE=1)
endif()

# Include submodule

set(SLEEF_SUBMODULE_INSTALL_DIR "${CMAKE_BINARY_DIR}/submodules")

include(ExternalProject)
include(FindPkgConfig)

if (NOT EXISTS "${PROJECT_SOURCE_DIR}/submodules")
  file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/submodules")
endif()

# Include SLEEF as a submodule

set(SLEEF_MINIMUM_VERSION 4.0.0)
set(SLEEF_GIT_TAG "0c063a8f0e01c22fa1e473effd2e7a0c69b4963a")

set(SLEEF_SOURCE_DIR "${PROJECT_SOURCE_DIR}/submodules/sleef")
set(SLEEF_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}")

set(SLEEF_CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${SLEEF_INSTALL_DIR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DSLEEF_BUILD_DFT=True -DSLEEF_ENFORCE_DFT=True -DSLEEFDFT_ENABLE_PARALLELFOR=True -DSLEEF_BUILD_TESTS=False -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=${CMAKE_POSITION_INDEPENDENT_CODE} -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS})

if (CMAKE_C_COMPILER)
  list(APPEND SLEEF_CMAKE_ARGS -DCMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER})
endif()

if (CMAKE_CXX_COMPILER)
  list(APPEND SLEEF_CMAKE_ARGS -DCMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER})
endif()

if (CMAKE_TOOLCHAIN_FILE)
  list(APPEND SLEEF_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

if (EXISTS "${SLEEF_SOURCE_DIR}/CMakeLists.txt")
  # If the source code of sleef is already downloaded, use it
  ExternalProject_Add(ext_sleef
    SOURCE_DIR "${SLEEF_SOURCE_DIR}"
    CMAKE_ARGS ${SLEEF_CMAKE_ARGS}
    UPDATE_DISCONNECTED TRUE
  )
  include_directories(BEFORE "${SLEEF_INSTALL_DIR}/include")
  link_directories(BEFORE "${SLEEF_INSTALL_DIR}/lib")
  set(SLEEF_LIBRARIES "sleefdft" "sleef" "tlfloat")
else()
  pkg_search_module(SLEEF sleef)

  if (SLEEF_FOUND AND SLEEF_VERSION VERSION_GREATER_EQUAL SLEEF_MINIMUM_VERSION)
    # If sleef is installed on the system
    add_custom_target(ext_sleef ALL)
    include_directories(BEFORE "${SLEEF_INCLUDE_DIRS}")
    link_directories(BEFORE "${SLEEF_LIBDIR}")
    message(STATUS "Found installed TLFloat " ${SLEEF_VERSION})
  else()
    # Otherwise, download the source code
    find_package(Git REQUIRED)
    ExternalProject_Add(ext_sleef
      GIT_REPOSITORY https://github.com/shibatch/sleef
      GIT_TAG "${SLEEF_GIT_TAG}"
      SOURCE_DIR "${SLEEF_SOURCE_DIR}"
      CMAKE_ARGS ${SLEEF_CMAKE_ARGS}
      UPDATE_DISCONNECTED TRUE
    )

    include_directories(BEFORE "${SLEEF_INSTALL_DIR}/include")
    link_directories(BEFORE "${SLEEF_INSTALL_DIR}/lib")
    set(SLEEF_LIBRARIES "sleefdft" "sleef" "tlfloat")
  endif()
endif()

if (NOT BUILD_SHARED_LIBS)
  add_compile_definitions(SLEEF_STATIC_LIBS=1)
endif()

#

if (NOT "${LC_CMAKE_BUILD_TYPE}" STREQUAL "release")
  add_compile_definitions(DEBUG=1)
endif()

#

add_subdirectory("src")

# Show status

if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
  message(STATUS "CMAKE_BUILD_TYPE : " ${CMAKE_BUILD_TYPE})
endif()
