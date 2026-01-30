cmake_minimum_required(VERSION 3.28)

project(Readback LANGUAGES CXX)

add_executable(${PROJECT_NAME} Readback.cxx)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
