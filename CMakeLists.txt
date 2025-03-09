cmake_minimum_required(VERSION 3.26)

#
# Setup project
#

project(RrFramework LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

include(CMake/CPM.cmake)

#
# Setup warnings and sanitizers
#

function(rr_enable_debug_options TARGET)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${TARGET} PRIVATE -Werror -Wall -Wextra -Wpedantic -Wno-unused-function -Wno-unused-parameter -Wno-missing-braces)

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            target_compile_options(${TARGET} PRIVATE -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unsafe-buffer-usage -Wno-pre-c++20-compat-pedantic)
        endif ()

        if (ENABLE_SANITIZERS)
            target_compile_options(${TARGET} PRIVATE -fsanitize=address,undefined)
            target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined)
        endif ()
    elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${TARGET} PRIVATE /W3)

        if (ENABLE_SANITIZERS)
            target_compile_options(${TARGET} PRIVATE /fsanitize=address)
        endif ()
    endif ()
endfunction ()

#
# Dependencies
#

find_package(Vulkan)

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

#
# Embed Function
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

file(GLOB_RECURSE RR_SOURCES CONFIGURE_DEPENDS
    "Source/*.c"
    "Source/*.cxx"
)

add_library(${PROJECT_NAME} OBJECT ${RR_SOURCES})

rr_enable_debug_options(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC Include)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE Vendor)

target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

target_sources(${PROJECT_NAME} PRIVATE Vendor/xxHash/xxhash.c Vendor/xxHash/xxh_x86dispatch.c)
target_sources(${PROJECT_NAME} PRIVATE Vendor/cJSON/cJSON.c)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Headers
    SDL3::SDL3
)

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
    PRIVATE
    "$<$<CONFIG:DEBUG>:-DRR_DEBUG>"
)

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

add_subdirectory(Demo)
add_subdirectory(Examples/01_Triangle)
add_subdirectory(Examples/05_GLTFCube)
add_subdirectory(Examples/10_BitonicSort)
add_subdirectory(Examples/11_PrefixSum)
add_subdirectory(Examples/99_GS)
