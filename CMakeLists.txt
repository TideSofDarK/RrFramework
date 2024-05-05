cmake_minimum_required(VERSION 3.26)

#
# Setup project
#
project(RrFramework LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

cmake_path(SET RR_BUILTIN_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_BUILTIN_ASSET_PATH NORMALIZE)

#
# Dependencies
#
find_package(Vulkan REQUIRED)

set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_FILE OFF CACHE BOOL "" FORCE)
set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
set(SDL_MISC OFF CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_3DNOW OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
CPMAddPackage("gh:libsdl-org/SDL#main")

CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#master")
CPMAddPackage("gh:zeux/volk#vulkan-sdk-1.3.280.0")

set(CGLM_STATIC ON CACHE BOOL "" FORCE)
CPMAddPackage("gh:recp/cglm#master")

add_subdirectory("Vendor/imgui/")

#
# Compile shaders
#
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(RR_SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/")
file(GLOB_RECURSE RR_GLSL_SOURCES CONFIGURE_DEPENDS
        "${RR_SHADER_DIR}*.vert.glsl"
        "${RR_SHADER_DIR}*.frag.glsl"
)

foreach (GLSL ${RR_GLSL_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME_WLE)
    set(SPIRV "${RR_SHADER_DIR}/${FILE_NAME}.spv")
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
        "Source/*.cpp"
)
add_library(${PROJECT_NAME} ${RR_SOURCES})
add_dependencies(${PROJECT_NAME} RrBuiltinShaders)
target_sources(${PROJECT_NAME} PRIVATE Vendor/cJSON/cJSON.c)
target_include_directories(${PROJECT_NAME} PUBLIC Include PRIVATE Include/Rr)
target_link_libraries(${PROJECT_NAME} PRIVATE
        Vulkan::Headers
        volk::volk_headers
        GPUOpen::VulkanMemoryAllocator
)
target_link_libraries(${PROJECT_NAME} PUBLIC
        SDL3::SDL3
        cglm
        ${IMGUI_TARGET}
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
        TINYEXR_USE_MINIZ=0
        TINYEXR_USE_STB_ZLIB=1
        STBI_NO_STDIO
        STBI_NO_JPEG
        STBI_NO_GIF
        STBI_NO_BMP
        STBI_NO_PSD
        STBI_NO_PIC
        STBI_NO_PNM
        STBI_NO_HDR
        STBI_NO_TGA
        STBI_NO_FAILURE_STRINGS
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    RR_BUILTIN_ASSET_PATH=${RR_BUILTIN_ASSET_PATH}
    "$<$<CONFIG:DEBUG>:-DRR_DEBUG>")
target_compile_features(${PROJECT_NAME} PRIVATE c_std_99 cxx_std_11)

add_subdirectory(Demo)
