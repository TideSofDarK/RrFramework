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
set(ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE)
set(RR_ASSET_PATH "${ASSET_PATH}")
add_subdirectory(Vendor/Rr)

#
# Compile shaders
#
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/")
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS
        "${SHADER_DIR}*.frag" "${SHADER_DIR}*.vert" "${SHADER_DIR}*.comp"
)

foreach (GLSL ${GLSL_SOURCE_FILES})
    # message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SHADER_DIR}/${FILE_NAME}.spv")
    # message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

#
# Add executable target
#
add_executable(${PROJECT_NAME} Source/AssetDef.c Source/Main.c Source/App.c)
set_source_files_properties(Source/Main.c PROPERTIES OBJECT_DEPENDS "${SPIRV_BINARY_FILES}")
add_dependencies(${PROJECT_NAME} Shaders)
target_link_libraries(${PROJECT_NAME} PRIVATE RetroRenderer)
if (WIN32)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()
target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /W3)
    endif ()
endif ()
