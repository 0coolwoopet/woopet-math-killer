cmake_minimum_required(VERSION 3.21)

# Tell CMake where to look for FindGeode.cmake before running find_package
if(DEFINED ENV{GEODE_SDK})
    list(APPEND CMAKE_MODULE_PATH "$ENV{GEODE_SDK}/cmake")
endif()

project(woopet.fucker VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Now CMake can cleanly locate the package
find_package(Geode REQUIRED)

add_library(${PROJECT_NAME} SHARED src/main.cpp)
setup_geode_mod(${PROJECT_NAME})
