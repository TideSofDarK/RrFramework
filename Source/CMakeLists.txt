add_executable(${PROJECT_NAME} main.c deps.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC Vendor)
target_compile_features(${PROJECT_NAME} PUBLIC c_std_23 cxx_std_20)
