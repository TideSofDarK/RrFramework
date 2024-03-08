cmake_minimum_required(VERSION 3.26)
project(RetroRenderer LANGUAGES C CXX)

add_subdirectory(Vendor)

set(BUILTIN_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH BUILTIN_ASSET_PATH NORMALIZE)
set(RR_BUILTIN_ASSET_PATH "${BUILTIN_ASSET_PATH}")

add_library(${PROJECT_NAME} STATIC Source/Rr/Rr.c Source/Rr/RrArray.c Source/Rr/RrAsset.c Source/Rr/RrDescriptor.c Source/Rr/RrApp.c)
target_include_directories(${PROJECT_NAME} PUBLIC Include PRIVATE Include/Rr)
target_link_libraries(${PROJECT_NAME} PUBLIC ${VENDOR_TARGET})
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_BUILTIN_ASSET_PATH=${RR_BUILTIN_ASSET_PATH} RR_ASSET_PATH=${RR_ASSET_PATH} "$<$<CONFIG:DEBUG>:-DRR_DEBUG>")
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11 cxx_std_17)
