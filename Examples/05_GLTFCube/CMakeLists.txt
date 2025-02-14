cmake_minimum_required(VERSION 3.26)

#
# Setup project
#

project(05_GLTFCube LANGUAGES C)

#
# Add executable target
#

add_executable(${PROJECT_NAME} 05_GLTFCube.c)

target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)

file(GLOB_RECURSE RR_EXAMPLE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.glb"
)

rr_embed_assets(
    ${PROJECT_NAME}
    "EXAMPLE_ASSET_"
    "ExampleAssets"
    "${RR_EXAMPLE_ASSETS}"
)
