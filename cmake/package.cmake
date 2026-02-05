function(package_slides_app)
    if (WIN32)
        set_target_properties(chowdsp_slides PROPERTIES WIN32_EXECUTABLE YES)
        target_compile_definitions(chowdsp_slides
            PUBLIC
                NOMINMAX=1
                _USE_MATH_DEFINES=1
        )
    elseif (APPLE)
        set(PROGRAM_NAME chowdsp_slides)
        configure_file(cmake/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/chowdsp-slides.plist)
        set_target_properties(chowdsp_slides PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/chowdsp-slides.plist
        )
    elseif (EMSCRIPTEN)
        file(GLOB asset_files ${CMAKE_CURRENT_SOURCE_DIR}/slides/assets/*)
        set(preload_commands "")
        foreach(asset_file ${asset_files})
            string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/slides/" "" asset_file_stem ${asset_file})
            message(STATUS "Packaging asset file: ${asset_file_stem}")
        set(preload_cmd "SHELL:--preload-file \"${asset_file}@${asset_file_stem}\"")
            list(APPEND preload_commands "${preload_cmd}")
        endforeach()

        target_link_options(chowdsp_slides
            PRIVATE
            --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/slides/slides.html
            ${preload_commands}
            -sGL_ENABLE_GET_PROC_ADDRESS
            -sALLOW_MEMORY_GROWTH
            --bind
            "-sEXPORTED_FUNCTIONS=['_main', '_pasteCallback']"
            "-sEXPORTED_RUNTIME_METHODS=['ccall', 'cwrap', 'UTF8ToString']"
        )

        set_target_properties(chowdsp_slides PROPERTIES
            SUFFIX ".html"
            OUTPUT_NAME "index"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/slides/web"
        )
    endif ()
endfunction(package_slides_app)
