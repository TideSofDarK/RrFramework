cmake_minimum_required(VERSION 3.28)

#
# Setup project
#

project(UITest LANGUAGES CXX)

#
# Add executable target
#

add_executable(${PROJECT_NAME} UITest.cxx)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
