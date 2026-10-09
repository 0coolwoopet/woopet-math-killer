cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(WoopetMathFucker VERSION 1.2.0)

# The Geode build action installs the SDK and exports GEODE_SDK.
# Geode projects should add that SDK as a subdirectory instead of using
# find_package(Geode), which cannot find GeodeConfig.cmake in this setup.
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please set GEODE_SDK to your Geode SDK directory, or build with geode-sdk/build-geode-mod.")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

message(STATUS "Using Geode SDK: $ENV{GEODE_SDK}")
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})
