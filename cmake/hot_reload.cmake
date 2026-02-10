function(hot_reload_slides target target_dir)
    message(STATUS "Configuring hot reloading for ${target}...")
    set(reload_target "${target}_reload")
    add_library(${reload_target} SHARED)
    target_sources(${reload_target} PRIVATE ${target_dir}/slides.cpp)
    target_link_libraries(${reload_target} PRIVATE visage)
    target_include_directories(${reload_target} PRIVATE ${target_dir} ${chowdsp_slides_dir}/src)

    set(dll_path ${CMAKE_BINARY_DIR}/slides_reload.dll)
    target_compile_definitions(${target}
        PRIVATE
            ALLOW_HOT_RELOAD=1
            BUILD_DIR="${CMAKE_BINARY_DIR}"
            DLL_PATH="${dll_path}"
            DLL_SOURCE="${target_dir}/slides.cpp"
            RELOAD_TARGET="${reload_target}"
    )

    add_custom_command(
        TARGET ${reload_target}
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND echo "Copying $<TARGET_FILE:${reload_target}> to ${dll_path}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${reload_target}>" "${dll_path}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE:${reload_target}>"
    )
endfunction()
