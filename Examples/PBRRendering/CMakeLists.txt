cmake_minimum_required(VERSION 3.28)

project(PBRRendering LANGUAGES CXX)

include(CPM)
CPMAddPackage(
    NAME
    webp
	URL
    https://github.com/webmproject/libwebp/archive/refs/tags/v1.6.0.zip
	OPTIONS
    "WEBP_BUILD_ANIM_UTILS OFF"
    "WEBP_BUILD_CWEBP OFF"
    "WEBP_BUILD_DWEBP OFF"
    "WEBP_BUILD_EXTRAS OFF"
    "WEBP_BUILD_GIF2WEBP OFF"
    "WEBP_BUILD_IMG2WEBP OFF"
    "WEBP_BUILD_LIBWEBPMUX OFF"
    "WEBP_BUILD_VWEBP OFF"
    "WEBP_BUILD_WEBPINFO OFF"
    "WEBP_BUILD_WEBPMUX OFF"
)

add_executable(${PROJECT_NAME} PBRRendering.cxx)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework webpdecode webpdspdecode webputilsdecode)

file(GLOB_RECURSE RR_EXAMPLE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.glb"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.glsl"
)
rr_embed_assets(
    ${PROJECT_NAME}
    "EXAMPLE_ASSET_"
    "ExampleAssets"
    "${RR_EXAMPLE_ASSETS}"
)
