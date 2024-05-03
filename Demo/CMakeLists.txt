#
# Setup project
#
cmake_minimum_required(VERSION 3.26)
project(RrDemo LANGUAGES C CXX)

#
# Setup Rr
#
cmake_path(SET RR_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_ASSET_PATH NORMALIZE)
#add_subdirectory(../Vendor/Rr)

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
set_source_files_properties(Source/AssetDef.c PROPERTIES OBJECT_DEPENDS "${RR_DEMO_GLSL_SOURCES}")
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "Source/*.c"
)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${PROJECT_NAME} RrDemoShaders)
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_ASSET_PATH=${RR_ASSET_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_17 cxx_std_17)
if (WIN32)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

        if (ENABLE_SANITIZERS)
            target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
            target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
        endif ()
    elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /W3)

        if (ENABLE_SANITIZERS)
            target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address)
        endif ()
    endif ()
endif ()
