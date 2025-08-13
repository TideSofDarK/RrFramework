cmake_minimum_required(VERSION 3.28)

#
# Setup project
#

project(UITest LANGUAGES C)

#
# Add executable target
#

add_executable(${PROJECT_NAME} UITest.c)

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES C_EXTENSIONS OFF)

target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
