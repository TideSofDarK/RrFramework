cmake_minimum_required(VERSION 3.28)

project(UITest LANGUAGES C)

add_executable(${PROJECT_NAME} UITest.c)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)

file(GLOB_RECURSE RR_EXAMPLE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
)
rr_embed_assets(
    ${PROJECT_NAME}
    "EXAMPLE_ASSET_"
    "ExampleAssets"
    "${RR_EXAMPLE_ASSETS}"
)
