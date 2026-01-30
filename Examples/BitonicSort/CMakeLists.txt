cmake_minimum_required(VERSION 3.28)

project(BitonicSort LANGUAGES CXX)

add_executable(${PROJECT_NAME} BitonicSort.cxx)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)

file(GLOB_RECURSE RR_EXAMPLE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.glsl"
)
rr_embed_assets(
    ${PROJECT_NAME}
    "EXAMPLE_ASSET_"
    "ExampleAssets"
    "${RR_EXAMPLE_ASSETS}"
)
