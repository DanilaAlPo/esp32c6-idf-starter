# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PROJECT_VER)
    set(PROJECT_VER "0.1.0-dev" CACHE STRING "Project version")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sample_project_cpp VERSION "${PROJECT_VER}")
