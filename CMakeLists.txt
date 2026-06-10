cmake_minimum_required(VERSION 3.28)

#
# Setup project
#

project(RrFramework LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

#
# Dependencies
#

find_package(Vulkan REQUIRED)

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
        "-DASSET_HEADER_NAME=${HEADER_NAME}"
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
            COMPILE_DEFINITIONS "RR_INCBIN_ASSETS=1"
        )
    endif ()

    target_sources(${TARGET_NAME} PUBLIC "${ASSET_HEADER_PATH}")
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/AssetHeaders/")
    add_dependencies(${TARGET_NAME} ${ASSETS_TARGET_NAME})
endfunction()

#
# Add library
#

add_library(${PROJECT_NAME} OBJECT)

target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<STREQUAL:$<COMPILE_LANGUAGE>,C>:${RR_C_FLAGS}>)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<STREQUAL:$<COMPILE_LANGUAGE>,CXX>:${RR_CXX_FLAGS}>)
set_target_properties(${PROJECT_NAME} PROPERTIES C_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)

target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC
    "$<$<CONFIG:DEBUG>:-DRR_DEBUG>"
)

target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC
    "$<$<CONFIG:DEBUG>:-DRR_USE_GPU_DEBUG_UTILS>"
)

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    Source/Rr_App.c
    Source/Rr_Arena.c
    Source/Rr_Asset.c
    Source/Rr_Buffer.c
    Source/Rr_Dependencies.cxx
    Source/Rr_Descriptor.c
    Source/Rr_Graph.c
    Source/Rr_Image.c
    Source/Rr_Log.c
    Source/Rr_Memory.c
    Source/Rr_Pipeline.c
    Source/Rr_Platform.c
    Source/Rr_Profiler.c
    Source/Rr_Renderer.c
    Source/Rr_SPIRV.c
    Source/Rr_Serialization.c
    Source/Rr_System.c
    Source/Rr_Thread.c
    Source/Rr_UI.c
    Source/Rr_Vulkan.c
    Vendor/xxHash/xxhash.c
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS Include
    FILES
    Include/Rr/Rr.h
    Include/Rr/Rr_App.h
    Include/Rr/Rr_Arena.h
    Include/Rr/Rr_Asset.h
    Include/Rr/Rr_Buffer.h
    Include/Rr/Rr_Defines.h
    Include/Rr/Rr_Graph.h
    Include/Rr/Rr_Image.h
    Include/Rr/Rr_Log.h
    Include/Rr/Rr_Math.h
    Include/Rr/Rr_Pipeline.h
    Include/Rr/Rr_Platform.h
    Include/Rr/Rr_Renderer.h
    Include/Rr/Rr_Serialization.h
    Include/Rr/Rr_System.h
    Include/Rr/Rr_Thread.h
    Include/Rr/Rr_UI.h
    Include/Rr/Rr_Utility.h
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    target_sources(${PROJECT_NAME} PRIVATE Vendor/xxHash/xxh_x86dispatch.c)
endif ()

set(RR_PLATFORM "SDL" CACHE STRING "Which platform backend to use")

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_System_UNIX.c)
    target_link_libraries(${PROJECT_NAME} PUBLIC -lc++)
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Metal" "-framework MetalKit" "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${Vulkan_MoltenVK_LIBRARY}")
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_SYSTEM_MACOS)

    set_property(CACHE RR_PLATFORM PROPERTY STRINGS "SDL")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_System_UNIX.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_SYSTEM_LINUX)

    set_property(CACHE RR_PLATFORM PROPERTY STRINGS "SDL" "XCB")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_System_Win32.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_SYSTEM_WIN32)

    set_property(CACHE RR_PLATFORM PROPERTY STRINGS "SDL" "Win32")
endif ()

if (RR_PLATFORM STREQUAL "Win32")
    target_link_libraries(${PROJECT_NAME} PRIVATE dxgi)
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_Platform_Win32.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_PLATFORM_WIN32)
endif ()

if (RR_PLATFORM STREQUAL "XCB")
    find_package(X11 REQUIRED)

    target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        X11::xkbcommon
        X11::xkbcommon_X11
        X11::xcb
        X11::xcb_cursor
        X11::xcb_xkb
        X11::xcb_randr)
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_Platform_XCB.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_PLATFORM_XCB)
endif ()

if (RR_PLATFORM STREQUAL "SDL")
    include(CPM)

    CPMAddPackage(
        URI
        "gh:libsdl-org/SDL#main"
	    OPTIONS
        "SDL_AUDIO OFF"
        "SDL_CAMERA OFF"
        "SDL_SENSOR OFF"
        "SDL_MISC OFF"
        "SDL_DIRECTX OFF"
        "SDL_OPENGLES OFF"
        "SDL_OPENGL OFF"
        "SDL_TEST_LIBRARY OFF"
        "SDL_GPU OFF"
        "SDL_RENDER OFF"
        "SDL_POWER OFF"
        "SDL_DISABLE_INSTALL ON"
        "SDL_DISABLE_UNINSTALL ON"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
    target_sources(${PROJECT_NAME} PRIVATE Source/Rr_Platform_SDL.c)
    target_compile_definitions(${PROJECT_NAME} PUBLIC RR_PLATFORM_SDL)
endif ()

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE Vendor)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers)

file(GLOB_RECURSE RR_BUILTIN_ASSETS CONFIGURE_DEPENDS
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

option(RR_EXAMPLES "Build included examples" OFF)
if (RR_EXAMPLES)
    add_subdirectory(Examples)
endif ()

#
# Tests
#

option(RR_TESTS "Build included tests" OFF)
if (RR_TESTS)
    add_subdirectory(Test)
endif ()
