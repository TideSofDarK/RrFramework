#
# Setup project
#
cmake_minimum_required(VERSION 3.26)
project(RrDemo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

#
# Setup warnings and sanitizers
#
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-missing-braces)

        if (ENABLE_SANITIZERS)
            add_compile_options(-fsanitize=address,undefined)
            add_link_options(-fsanitize=address,undefined)
        endif ()
    elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
        add_compile_options(/W3)

        if (ENABLE_SANITIZERS)
            target_compile_options(/fsanitize=address)
        endif ()
    endif ()
endif ()

#
# Setup RrFramework
#
cmake_path(SET RR_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_ASSET_PATH NORMALIZE)

#
# Compile shaders
#
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/Shader/")
set(RR_DEMO_GLSL_SOURCES
        ${SHADER_DIR}Uber3D.vert.glsl
        ${SHADER_DIR}Uber3D.frag.glsl
)

foreach (GLSL ${RR_DEMO_GLSL_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME_WLE)
    set(SPIRV "${SHADER_DIR}/${FILE_NAME}.spv")
    add_custom_command(
            MAIN_DEPENDENCY ${GLSL}
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -gVS --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            VERBATIM)
    list(APPEND RR_DEMO_SPIRV_BINARIES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        RrDemoShaders
        DEPENDS ${RR_DEMO_SPIRV_BINARIES}
)

#
# Add executable target
#
set_source_files_properties(Source/AssetDef.cxx PROPERTIES OBJECT_DEPENDS "${RR_DEMO_GLSL_SOURCES}")
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "Source/*.cxx"
)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${PROJECT_NAME} RrDemoShaders)
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_ASSET_PATH=${RR_ASSET_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
if (WIN32)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()