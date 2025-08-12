cmake_minimum_required(VERSION 3.27)
project(andex C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")

  add_compile_options(-O3 -DNDEBUG)

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL
                                                 Emscripten)
    add_compile_options(-march=native -mtune=native)
    add_compile_options(-flto)
    add_link_options(-flto)
    add_compile_options(-fomit-frame-pointer -fno-stack-protector)
  endif()

  if(MSVC)
    add_compile_options(/O2 /GL)
    add_link_options(/LTCG)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
endif()

set(CLAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/clay)
set(SOKOL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sokol)
set(HMM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/hmm)
set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/stb)
set(NANOSVG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/nanosvg)
set(SOKOL_GP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sokol_gp)
set(FONTSTASH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/fontstash/src)
set(TFD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/tfd)

set(FONTSTASH_HEADERS ${FONTSTASH_DIR}/fontstash.h)

set(SOKOL_HEADERS
    ${SOKOL_DIR}/sokol_app.h
    ${SOKOL_DIR}/sokol_gfx.h
    ${SOKOL_DIR}/sokol_glue.h
    ${SOKOL_DIR}/sokol_log.h
    ${SOKOL_DIR}/sokol_time.h
    ${SOKOL_DIR}/sokol_fetch.h
    ${SOKOL_DIR}/util/sokol_gl.h
    ${SOKOL_DIR}/util/sokol_fontstash.h
    ${FONTSTASH_HEADERS})

add_library(sokol INTERFACE ${SOKOL_HEADERS})

target_include_directories(sokol INTERFACE ${SOKOL_DIR} ${SOKOL_DIR}/util
                                           ${FONTSTASH_DIR})

find_program(
  SOKOL_SHDC
  NAMES sokol-shdc sokol-shdc.exe
  PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sokol-tools/bin/osx
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sokol-tools/bin/linux
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/sokol-tools/bin/win32 ENV PATH
  DOC "sokol-shdc shader compiler")

if(NOT SOKOL_SHDC)
  message(
    WARNING
      "sokol-shdc not found. You can download it from https://github.com/floooh/sokol-tools-bin"
  )
endif()

function(compile_shader SHADER_FILE OUTPUT_FILE SLANG)
  add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND ${SOKOL_SHDC} --input ${SHADER_FILE} --output ${OUTPUT_FILE} --slang
            ${SLANG}
    DEPENDS ${SHADER_FILE}
    COMMENT "Compiling shader ${SHADER_FILE}"
    VERBATIM)
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL Emscripten)
  set(SHADER_LANG "wgsl")
  add_compile_definitions(SOKOL_WGPU)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(SHADER_LANG "metal_macos")
  add_compile_definitions(SOKOL_METAL)
elseif(CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(SHADER_LANG "hlsl5")
  add_compile_definitions(SOKOL_D3D11)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(SHADER_LANG "glsl410")
  add_compile_definitions(SOKOL_GLCORE)
else()
  set(SHADER_LANG "glsl410")
  add_compile_definitions(SOKOL_GLCORE)
endif()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${GENERATED_DIR})

compile_shader(${SHADER_DIR}/shaders.glsl ${GENERATED_DIR}/shaders.h
               ${SHADER_LANG})

set(GENERATED_SHADERS ${GENERATED_DIR}/shaders.h)

set(ANDEX_SOURCES src/svg.c src/buffer.c src/editor.c src/main.c ${GENERATED_SHADERS})

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  list(APPEND ANDEX_SOURCES src/files.c src/mac_window.c)
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
  list(APPEND ANDEX_SOURCES src/files.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  add_executable(andex WIN32 ${ANDEX_SOURCES})
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT
                                                              andex)
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

  add_executable(andex MACOSX_BUNDLE ${ANDEX_SOURCES})

  set_target_properties(
    andex
    PROPERTIES MACOSX_BUNDLE TRUE
               MACOSX_BUNDLE_INFO_PLIST
               ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
               MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourcompany.andex"
               MACOSX_BUNDLE_BUNDLE_NAME "Andex"
               MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
               MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")

else()
  add_executable(andex ${ANDEX_SOURCES})
endif()

target_include_directories(
  andex
  PRIVATE ${CLAY_DIR}
          ${CLAY_DIR}/renderers/sokol
          ${SOKOL_DIR}
          ${SOKOL_DIR}/util
          ${FONTSTASH_DIR}
          ${GENERATED_DIR}
          ${HMM_DIR}
          ${STB_DIR}
          ${NANOSVG_DIR}/src
          ${SOKOL_GP_DIR}
          ${TFD_DIR}
          src)

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
  target_compile_options(andex PRIVATE -x objective-c)
  target_link_libraries(
    andex
    PUBLIC sokol "-framework QuartzCore" "-framework Cocoa"
           "-framework MetalKit" "-framework Metal")
else()
  target_link_libraries(andex PUBLIC sokol)
  if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_compile_definitions(andex PRIVATE SOKOL_GLCORE=1)
    target_link_libraries(andex PUBLIC X11 Xi Xcursor GL dl m)
    target_link_libraries(andex PUBLIC Threads::Threads)
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Emscripten)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)
    target_link_options(andex PRIVATE --shell-file
                        ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)
  endif()

  target_compile_options(andex PRIVATE --use-port=emdawnwebgpu)
  target_link_options(andex PRIVATE --use-port=emdawnwebgpu)

  # Export FS and ccall for EM_JS bridges
  target_link_options(andex PRIVATE
    -sFORCE_FILESYSTEM=1
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
    "-sEXPORTED_FUNCTIONS=['_pick__deliver_single','_pick__deliver_multi_lines','_pick__deliver_msg','_main']"
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(andex PRIVATE -O3 -sINITIAL_MEMORY=50331648
                        -sASSERTIONS=0 -sMALLOC=emmalloc)
  else()
    target_link_options(andex PRIVATE -sINITIAL_MEMORY=50331648 -sASSERTIONS=2
                        -sMALLOC=emmalloc)
  endif()
endif()


if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL
                                           Emscripten)
  target_link_options(andex PRIVATE LINKER:-dead_strip)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release"
   AND NOT MSVC
   AND NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten)
  if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_link_options(andex PRIVATE -Wl,-S)
  elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_link_options(andex PRIVATE -s)
  endif()
endif()

set_target_properties(andex PROPERTIES LINKER_LANGUAGE C)

if(NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  add_custom_command(
    TARGET andex
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources
      ${CMAKE_CURRENT_BINARY_DIR}/resources)
endif()
