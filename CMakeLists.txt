#
# Setup project
#
cmake_minimum_required(VERSION 3.26)
project(VulkanPlayground LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

set(CMAKE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

#
# Setup Rr
#
cmake_path(SET RR_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_ASSET_PATH NORMALIZE)
add_subdirectory(Vendor/Rr)

#
# Compile shaders
#
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/Shader/")
file(GLOB GLSL_SOURCE_FILES CONFIGURE_DEPENDS
        "${SHADER_DIR}*.glsl"
)

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME_WLE)
    set(SPIRV "${SHADER_DIR}/${FILE_NAME}.spv")
    add_custom_command(
            MAIN_DEPENDENCY ${GLSL}
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            VERBATIM)
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

#
# Add executable target
#
set_source_files_properties(Source/AssetDef.c PROPERTIES OBJECT_DEPENDS "${GLSL_SOURCE_FILES}")
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "Source/*.c"
)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
add_dependencies(${PROJECT_NAME} Shaders)
target_link_libraries(${PROJECT_NAME} PRIVATE RetroRenderer)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
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
