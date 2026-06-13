cmake_minimum_required(VERSION 3.28)

project(AllocatorTest LANGUAGES C)

add_executable(${PROJECT_NAME} AllocatorTest.c)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
