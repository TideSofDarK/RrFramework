add_executable(${PROJECT_NAME} main.c app.c renderer.c deps.cxx)
target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}Vendor)
