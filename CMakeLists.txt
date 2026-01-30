cmake_minimum_required(VERSION 3.17)
set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
set(CMAKE_CXX_STANDARD 20)

project(chowdsp-slides VERSION 0.1.0)

add_subdirectory(visage)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_executable(chowdsp_slides src/slides_main.cpp)
target_link_libraries(chowdsp_slides PRIVATE visage)
target_include_directories(chowdsp_slides PRIVATE slides src)

add_library(slides_reload SHARED)
target_sources(slides_reload PRIVATE slides/slides.cpp)
# @TODO: don't fully link here? The include directories should be enough
target_link_libraries(slides_reload PRIVATE visage)
target_include_directories(slides_reload PRIVATE slides src)
# @TODO: define output file path for original target (and probably build dir also)

include(package)
package_slides_app()
