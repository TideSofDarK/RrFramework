cmake_minimum_required(VERSION 3.26)

#
# Setup project
#

project(RrDemo LANGUAGES CXX)

#
# Assets
#

file(GLOB_RECURSE RR_DEMO_ASSETS CONFIGURE_DEPENDS
    "${RR_ASSET_PATH}*.png"
    "${RR_ASSET_PATH}*.exr"
    "${RR_ASSET_PATH}*.glb"
    "${SHADER_DIR}Uber3D.vert.glsl"
    "${SHADER_DIR}Uber3D.frag.glsl"
)

#
# Add executable target
#

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "Source/*.cxx"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

rr_enable_debug_options(${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_compile_definitions(${PROJECT_NAME} PUBLIC RR_ASSET_PATH=${RR_ASSET_PATH})

target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)

if (WIN32)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()

file(GLOB_RECURSE RR_DEMO_BUILTIN_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.obj"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.exr"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.glb"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.frag.glsl"
)

rr_embed_assets(
    ${PROJECT_NAME}
    "RrDemoBuiltinAssets"
    "DEMO_ASSET_"
    "DemoAssets"
    "${RR_DEMO_BUILTIN_ASSETS}"
)
