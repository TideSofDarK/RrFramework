cmake_minimum_required(VERSION 3.28)

#
# Setup project
#

project(RrFramework LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

include(CMake/CPM.cmake)

#
# Enable warnings and sanitizers
#

function(rr_enable_development_options TARGET)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(
            ${TARGET}
            PRIVATE
            -Werror
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-function
            -Wno-unused-parameter
            -Wno-unused-variable
            -Wno-missing-braces)

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            target_compile_options(${TARGET} PRIVATE -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unsafe-buffer-usage -Wno-pre-c++20-compat-pedantic)
        endif ()

        if (RR_SANITIZERS)
            target_compile_options(${TARGET} PRIVATE -fsanitize=address,undefined)
            target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined)
        endif ()
    elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${TARGET} PRIVATE /W3)

        if (RR_SANITIZERS)
            target_compile_options(${TARGET} PRIVATE /fsanitize=address)
        endif ()
    endif ()
endfunction ()

#
# Dependencies
#

find_package(Vulkan)

option(RR_PLATFORM_SDL "" ON)
if (RR_PLATFORM_SDL)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_MISC OFF CACHE BOOL "" FORCE)
    set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    CPMAddPackage("gh:libsdl-org/SDL#main")
endif ()

#
# Pre-C23 asset embedding support
#

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
find_program(GLSLC glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND CMAKE_C_SIMULATE_ID STREQUAL "")
    set(RR_USE_RC 1)
    add_compile_definitions("RR_USE_RC=1")
else ()
    set(RR_USE_RC 0)
endif ()

function(rr_embed_assets TARGET_NAME IDENTIFIER_PREFIX HEADER_NAME ASSETS_LIST)
    set(ASSET_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/AssetHeaders/${HEADER_NAME}.inc")

    set(ASSETS_LIST_FINAL "")

    # Compile shaders
    set(SPIRVBinaries "")
    foreach (ASSET_PATH ${ASSETS_LIST})
        get_filename_component(ASSET_EXTENSION ${ASSET_PATH} EXT)
        if ((ASSET_EXTENSION STREQUAL ".vert.glsl") OR (ASSET_EXTENSION STREQUAL ".frag.glsl") OR (ASSET_EXTENSION STREQUAL ".comp.glsl"))
            get_filename_component(ASSET_NAME_WLE ${ASSET_PATH} NAME_WLE)
            set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/${ASSET_NAME_WLE}.spv")
            add_custom_command(
                MAIN_DEPENDENCY ${ASSET_PATH}
                OUTPUT ${SPIRV}
                COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.0 ${ASSET_PATH} -o ${SPIRV}
                DEPENDS ${ASSET_PATH}
                VERBATIM)
            list(APPEND SPIRVBinaries ${SPIRV})
            set(ASSET_PATH ${SPIRV})
        endif ()
        list(APPEND ASSETS_LIST_FINAL "${ASSET_PATH}")
    endforeach (ASSET_PATH)

    set(ASSETS_LIST "${ASSETS_LIST_FINAL}")

    add_custom_command(
        OUTPUT
        "${ASSET_HEADER_PATH}"
        COMMAND ${CMAKE_COMMAND}
        "-DASSET_HEADER_PATH=${ASSET_HEADER_PATH}"
        "-DASSETS_LIST=${ASSETS_LIST}"
        "-DIDENTIFIER_PREFIX=${IDENTIFIER_PREFIX}"
        "-DUSE_RC=${RR_USE_RC}"
        -P ${RrFramework_SOURCE_DIR}/CMake/EmbedAssets.cmake
        DEPENDS
        ${ASSETS_LIST}
        VERBATIM
    )

    set(ASSETS_TARGET_NAME "${TARGET_NAME}EmbeddedAssets")

    add_custom_target(
        ${ASSETS_TARGET_NAME}
        DEPENDS
        "${ASSET_HEADER_PATH}"
    )

    set_source_files_properties("${ASSET_HEADER_PATH}" PROPERTIES OBJECT_DEPENDS "${ASSETS_LIST}")
    set_source_files_properties("${ASSET_HEADER_PATH}" PROPERTIES HEADER_FILE_ONLY NO)

    if (RR_USE_RC)
        set_source_files_properties("${ASSET_HEADER_PATH}" PROPERTIES LANGUAGE RC)
    else ()
        set_source_files_properties("${ASSET_HEADER_PATH}" PROPERTIES LANGUAGE C)
        set_source_files_properties(
            "${ASSET_HEADER_PATH}"
            PROPERTIES
            COMPILE_DEFINITIONS "RR_DEFINE_ASSETS=1"
        )
    endif ()

    target_sources(${TARGET_NAME} PUBLIC "${ASSET_HEADER_PATH}")
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/AssetHeaders/")
    add_dependencies(${TARGET_NAME} ${ASSETS_TARGET_NAME})
endfunction()

#
# Add library
#

add_library(${PROJECT_NAME} OBJECT ${RR_SOURCES})

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES C_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    TINYEXR_USE_MINIZ=0
    TINYEXR_USE_STB_ZLIB=1
    STBI_NO_STDIO
    STBI_NO_GIF
    STBI_NO_BMP
    STBI_NO_PSD
    STBI_NO_PIC
    STBI_NO_PNM
    STBI_NO_HDR
    STBI_NO_TGA
    STBI_NO_FAILURE_STRINGS
    VK_NO_PROTOTYPES
)

target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC
    "$<$<CONFIG:DEBUG>:-DRR_DEBUG>"
)

rr_enable_development_options(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    Source/Rr_App.c
    Source/Rr_Asset.c
    Source/Rr_Buffer.c
    Source/Rr_Dependencies.cxx
    Source/Rr_Descriptor.c
    Source/Rr_GLTF.c
    Source/Rr_Graph.c
    Source/Rr_Image.c
    Source/Rr_Load.c
    Source/Rr_Memory.c
    Source/Rr_Pipeline.c
    Source/Rr_Platform.c
    Source/Rr_Renderer.c
    Source/Rr_UI.c
    Source/Rr_Vulkan.c
    Vendor/xxHash/xxhash.c
    Vendor/xxHash/xxh_x86dispatch.c
    Vendor/cJSON/cJSON.c
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS Include
    FILES
    Include/Rr/Rr_App.h
    Include/Rr/Rr_Asset.h
    Include/Rr/Rr_Buffer.h
    Include/Rr/Rr_Defines.h
    Include/Rr/Rr_GLTF.h
    Include/Rr/Rr_Graph.h
    Include/Rr/Rr.h
    Include/Rr/Rr_Hive.h
    Include/Rr/Rr_Image.h
    Include/Rr/Rr_Load.h
    Include/Rr/Rr_Math.h
    Include/Rr/Rr_Memory.h
    Include/Rr/Rr_Pipeline.h
    Include/Rr/Rr_Platform.h
    Include/Rr/Rr_Renderer.h
    Include/Rr/Rr_UI.h
    Include/Rr/Rr_Utility.h
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_Platform_Linux.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_PLATFORM_LINUX)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_Platform_Win32.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_PLATFORM_WIN32)
endif ()

if (RR_PLATFORM_SDL)
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_Platform_SDL.c)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_PLATFORM_SDL)
endif ()

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE Vendor)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers)

file(GLOB_RECURSE RR_BUILTIN_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.frag.glsl"
)

rr_embed_assets(
    ${PROJECT_NAME}
    "RR_BUILTIN_"
    "Rr_BuiltinAssets"
    "${RR_BUILTIN_ASSETS}"
)

#
# Examples
#

option(RR_EXAMPLES "Build included examples" ON)
if (RR_EXAMPLES)
    add_subdirectory(Examples/Triangle)
    add_subdirectory(Examples/GLTFCube)
    add_subdirectory(Examples/QuadTree)
    add_subdirectory(Examples/SmoothGrid)
    add_subdirectory(Examples/PrerenderedDepth)
    add_subdirectory(Examples/Skybox)
    add_subdirectory(Examples/BitonicSort)
    add_subdirectory(Examples/PrefixSum)
    add_subdirectory(Examples/HiveTest)
    add_subdirectory(Examples/UITest)
    add_subdirectory(Examples/GS)
endif ()
