cmake_minimum_required(VERSION 3.17)
set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
set(CMAKE_CXX_STANDARD 20)

project(chowdsp-slides VERSION 0.1.0)

add_subdirectory(visage)

add_executable(chowdsp_slides main.cpp)
target_link_libraries(chowdsp_slides PRIVATE visage)

if (WIN32)
  set_target_properties(chowdsp_slides PROPERTIES WIN32_EXECUTABLE YES)
elseif (APPLE)
  set(PROGRAM_NAME chowdsp_slides)
  configure_file(Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/chowdsp-slides.plist)
  set_target_properties(chowdsp_slides PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/chowdsp-slides.plist
  )
elseif (EMSCRIPTEN)
  # target_link_options(chowdsp_slides
  #   PRIVATE
  #   --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/emscripten_template.html
  #   -sGL_ENABLE_GET_PROC_ADDRESS
  #   -sALLOW_MEMORY_GROWTH
  #   --bind
  #   "-sEXPORTED_FUNCTIONS=['_main', '_pasteCallback']"
  #   "-sEXPORTED_RUNTIME_METHODS=['ccall', 'cwrap', 'UTF8ToString']"
  # )

  # set_target_properties(chowdsp_slides PROPERTIES
  #   SUFFIX ".html"
  #   OUTPUT_NAME "index"
  #   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/builds/${EXAMPLE}"
  # )
endif ()
