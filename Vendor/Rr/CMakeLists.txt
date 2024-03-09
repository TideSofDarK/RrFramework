cmake_minimum_required(VERSION 3.26)
project(RetroRenderer LANGUAGES C CXX)

add_subdirectory(Vendor)

cmake_path(SET RR_BUILTIN_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_BUILTIN_ASSET_PATH NORMALIZE)

file(GLOB_RECURSE RR_SOURCES CONFIGURE_DEPENDS
    "Source/*.c"
)

add_library(${PROJECT_NAME} ${RR_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC Include PRIVATE Include/Rr)
target_link_libraries(${PROJECT_NAME} PUBLIC ${VENDOR_TARGET})
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_BUILTIN_ASSET_PATH=${RR_BUILTIN_ASSET_PATH} RR_ASSET_PATH=${RR_ASSET_PATH} "$<$<CONFIG:DEBUG>:-DRR_DEBUG>")
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_17)
