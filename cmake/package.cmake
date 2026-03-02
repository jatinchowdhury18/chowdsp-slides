function(slides_app target target_dir)

    add_executable(${target} ${chowdsp_slides_dir}/src/slides_main.cpp)
    target_link_libraries(${target} PRIVATE visage)
    target_include_directories(${target} PRIVATE ${target_dir} ${chowdsp_slides_dir}/src)

    set(js_sources "${chowdsp_slides_dir}/src/third_party/mathjax-bundle/mathjax-embedded.js")
    if(NOT EMSCRIPTEN)
        add_embedded_resources(embedded_js "embedded_js.h" "chowdsp::slides::resources::js" "${js_sources}")
        target_link_libraries(${target} PRIVATE embedded_js)
    endif()

    if (WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE YES)
        target_compile_definitions(${target}
            PUBLIC
                NOMINMAX=1
                _USE_MATH_DEFINES=1
        )
    elseif (APPLE)
        set(PROGRAM_NAME ${target})
        configure_file(${chowdsp_slides_dir}/cmake/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/chowdsp-slides.plist)
        set_target_properties(${target} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/chowdsp-slides.plist
        )
    elseif (EMSCRIPTEN)
        file(GLOB asset_files ${target_dir}/assets/*)
        set(preload_commands "")
        foreach(asset_file ${asset_files})
            string(REPLACE "${target_dir}/" "" asset_file_stem ${asset_file})
            message(STATUS "Packaging asset file: ${asset_file_stem}")
            set(preload_cmd "SHELL:--preload-file \"${asset_file}@${asset_file_stem}\"")
            list(APPEND preload_commands "${preload_cmd}")
        endforeach()
        list(APPEND preload_commands "SHELL:--preload-file \"${target_dir}/slides.gon@slides.gon\"")
        message(STATUS "Pre-load commands: ${preload_commands}")

        target_link_options(${target}
            PRIVATE
            --shell-file ${target_dir}/slides.html
            ${preload_commands}
            -sGL_ENABLE_GET_PROC_ADDRESS
            -sALLOW_MEMORY_GROWTH
            -sASYNCIFY
            --bind
            "-sEXPORTED_FUNCTIONS=['_main', '_pasteCallback']"
            "-sEXPORTED_RUNTIME_METHODS=['ccall', 'cwrap', 'UTF8ToString']"
        )

        set_target_properties(${target} PROPERTIES
            SUFFIX ".html"
            OUTPUT_NAME "index"
            RUNTIME_OUTPUT_DIRECTORY "${target_dir}/web"
        )

        add_custom_target(copy_js ALL
            COMMENT "Copying JS to ${target_dir}/web/src"
        )
        add_custom_command(
            TARGET copy_js
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${target_dir}/web/src"
            COMMENT "Deleting old JS folder: ${target_dir}/web/src"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${target_dir}/web/src"
            COMMENT "Creating new JS folder: ${target_dir}/web/src"
        )
        foreach(js_source ${js_sources})
            add_custom_command(
                TARGET copy_js POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${js_source}" "${target_dir}/web/src"
                COMMENT "Copying ${js_source} to ${target_dir}/web/src"
            )
        endforeach()
        add_dependencies(${target} copy_js)
    endif ()
endfunction()
