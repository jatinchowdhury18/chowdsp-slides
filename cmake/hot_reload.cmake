message(STATUS "Configuring hot reloading...")
add_library(slides_reload SHARED)
target_sources(slides_reload PRIVATE slides/slides.cpp)
target_link_libraries(slides_reload PRIVATE visage)
target_include_directories(slides_reload PRIVATE slides src)

set(dll_path $<TARGET_FILE:slides_reload>)
target_compile_definitions(chowdsp_slides
    PRIVATE
        ALLOW_HOT_RELOAD=1
        BUILD_DIR="${CMAKE_BINARY_DIR}"
        DLL_PATH="${dll_path}"
        DLL_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/slides/slides.cpp"
)
