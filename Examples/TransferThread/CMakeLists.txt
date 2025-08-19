cmake_minimum_required(VERSION 3.28)

#
# Setup project
#

project(TransferThread LANGUAGES CXX)

#
# Add executable target
#

add_executable(${PROJECT_NAME} TransferThread.cxx)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)

file(GLOB_RECURSE RR_EXAMPLE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
)

rr_embed_assets(
    ${PROJECT_NAME}
    "EXAMPLE_ASSET_"
    "ExampleAssets"
    "${RR_EXAMPLE_ASSETS}"
)
