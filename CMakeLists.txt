cmake_minimum_required(VERSION 3.17)
set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
set(CMAKE_CXX_STANDARD 20)

project(chowdsp-slides VERSION 0.1.0)

add_subdirectory(visage)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(chowdsp_slides_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(chowdsp_slides_dir "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)

include(package)
slides_app(chowdsp_slides "${CMAKE_CURRENT_SOURCE_DIR}/slides")

if (NOT EMSCRIPTEN)
    include(hot_reload)
endif()
