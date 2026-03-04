cmake_minimum_required(VERSION 3.17)
set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
set(CMAKE_CXX_STANDARD 20)

project(chowdsp-slides VERSION 0.1.0)

# Uncomment the next line to enable address sanitizer with Clang
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")

# Uncomment measure build times with Clang
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")

add_subdirectory(visage)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(chowdsp_slides_dir "${CMAKE_CURRENT_SOURCE_DIR}")
if (NOT PROJECT_IS_TOP_LEVEL)
    set(chowdsp_slides_dir "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
endif()

include(package)
slides_app(chowdsp_slides "${CMAKE_CURRENT_SOURCE_DIR}/slides")

if (NOT EMSCRIPTEN)
    target_compile_definitions(chowdsp_slides PRIVATE ALLOW_HOT_RELOAD=1)
endif()
