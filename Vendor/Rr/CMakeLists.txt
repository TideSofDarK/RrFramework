cmake_minimum_required(VERSION 3.26)
project(RetroRenderer LANGUAGES C CXX)

add_subdirectory(Vendor)

cmake_path(SET RR_BUILTIN_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_BUILTIN_ASSET_PATH NORMALIZE)

file(GLOB_RECURSE RR_SOURCES CONFIGURE_DEPENDS
    "Source/*.c"
)

#
# Compile shaders
#
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/")
set(GLSL_SOURCE_FILES
        ${SHADER_DIR}Text.vert.glsl
        ${SHADER_DIR}Text.frag.glsl
)

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME_WLE)
    set(SPIRV "${SHADER_DIR}/${FILE_NAME}.spv")
    add_custom_command(
            MAIN_DEPENDENCY ${GLSL}
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -gVS --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            VERBATIM)
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        RrShaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

#
# Add library
#
set_source_files_properties(Source/Rr_AssetDef.c PROPERTIES OBJECT_DEPENDS "${GLSL_SOURCE_FILES}")
add_library(${PROJECT_NAME} ${RR_SOURCES})
add_dependencies(${PROJECT_NAME} RrShaders)
target_include_directories(${PROJECT_NAME} PUBLIC Include PRIVATE Include/Rr)
target_link_libraries(${PROJECT_NAME} PUBLIC ${VENDOR_TARGET})
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_BUILTIN_ASSET_PATH=${RR_BUILTIN_ASSET_PATH} RR_ASSET_PATH=${RR_ASSET_PATH} "$<$<CONFIG:DEBUG>:-DRR_DEBUG>")
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_17)
