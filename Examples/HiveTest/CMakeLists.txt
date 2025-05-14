cmake_minimum_required(VERSION 3.28)

#
# Setup project
#

project(HiveTest LANGUAGES C)

#
# Add executable target
#

add_executable(${PROJECT_NAME} Main.c)

target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
