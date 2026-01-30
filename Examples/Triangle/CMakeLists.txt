cmake_minimum_required(VERSION 3.28)

project(Triangle LANGUAGES C)

add_executable(${PROJECT_NAME} Triangle.c)
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)

file(GLOB_RECURSE RR_EXAMPLE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.glsl"
)
rr_embed_assets(
    ${PROJECT_NAME}
    "EXAMPLE_ASSET_"
    "ExampleAssets"
    "${RR_EXAMPLE_ASSETS}"
)
