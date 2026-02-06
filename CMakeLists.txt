# StaticTeststandController - ESP-IDF Project
# Dual-unit embedded system for rocket motor static testing
cmake_minimum_required(VERSION 3.16)

# Build target: set to "BASE" or "REMOTE" before building
# Usage: BUILD_TARGET=BASE idf.py build
#    or: BUILD_TARGET=REMOTE idf.py build
# Also supports: idf.py -D BUILD_TARGET=BASE build
if(DEFINED ENV{BUILD_TARGET})
    set(BUILD_TARGET "$ENV{BUILD_TARGET}" CACHE STRING "Build target: BASE or REMOTE" FORCE)
elseif(NOT DEFINED BUILD_TARGET)
    set(BUILD_TARGET "BASE" CACHE STRING "Build target: BASE or REMOTE")
endif()

# Propagate to environment so component discovery subprocesses can see it
set(ENV{BUILD_TARGET} "${BUILD_TARGET}")

message(STATUS "Building for target: ${BUILD_TARGET}")

# Add compile definition for conditional compilation
add_compile_definitions(BUILD_TARGET_${BUILD_TARGET})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(StaticTeststandController)
