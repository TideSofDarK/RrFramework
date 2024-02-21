add_executable(${PROJECT_NAME} Main.c App.c Renderer.c Deps.cxx)
target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}Vendor)
if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wall -Wextra -Wpedantic)
endif ()
