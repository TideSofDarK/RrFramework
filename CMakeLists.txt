cmake_minimum_required(VERSION 3.26)
project(RrFramework LANGUAGES C CXX)

#
# Setup warnings and sanitizers
#
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-Wall -Wextra -Wpedantic)

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

set(BUILD_SHARED_LIBS OFF)

file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

add_subdirectory(Vendor)

cmake_path(SET RR_BUILTIN_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_BUILTIN_ASSET_PATH NORMALIZE)

#
# Compile shaders
#
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/")
file(GLOB_RECURSE RR_GLSL_SOURCES CONFIGURE_DEPENDS
        "Source/*.vert.glsl"
        "Source/*.frag.glsl"
)

foreach (GLSL ${RR_GLSL_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME_WLE)
    set(SPIRV "${SHADER_DIR}/${FILE_NAME}.spv")
    add_custom_command(
            MAIN_DEPENDENCY ${GLSL}
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -gVS --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            VERBATIM)
    list(APPEND RR_SPIRV_BINARIES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        RrBuiltinShaders
        DEPENDS ${RR_SPIRV_BINARIES}
)

#
# Add library
#
set_source_files_properties(Source/Rr_AssetDef.c PROPERTIES OBJECT_DEPENDS "${RR_GLSL_SOURCES}")
file(GLOB_RECURSE RR_SOURCES CONFIGURE_DEPENDS
        "Source/*.c"
)
add_library(${PROJECT_NAME} ${RR_SOURCES})
add_dependencies(${PROJECT_NAME} RrBuiltinShaders)
target_include_directories(${PROJECT_NAME} PUBLIC Include PRIVATE Include/Rr)
target_link_libraries(${PROJECT_NAME} PUBLIC ${VENDOR_TARGET})
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_BUILTIN_ASSET_PATH=${RR_BUILTIN_ASSET_PATH} "$<$<CONFIG:DEBUG>:-DRR_DEBUG>")
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_17)

add_subdirectory(Demo)
