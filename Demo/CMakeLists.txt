#
# Setup project
#
cmake_minimum_required(VERSION 3.26)
project(RrDemo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

#
# Setup warnings and sanitizers
#
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-missing-braces)

        if (ENABLE_SANITIZERS)
            add_compile_options(-fsanitize=address,undefined)
            add_link_options(-fsanitize=address,undefined)
        endif ()
    elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
        add_compile_options(/W3)

        if (ENABLE_SANITIZERS)
            target_compile_options(/fsanitize=address)
        endif ()
    endif ()
endif ()

#
# Setup RrFramework
#
cmake_path(SET RR_ASSET_PATH "Asset/")
cmake_path(ABSOLUTE_PATH RR_ASSET_PATH NORMALIZE)

#
# Assets
#
file(GLOB_RECURSE RR_DEMO_ASSETS CONFIGURE_DEPENDS
        "${RR_ASSET_PATH}*.png"
        "${RR_ASSET_PATH}*.exr"
        "${RR_ASSET_PATH}*.glb"
        "${SHADER_DIR}Uber3D.vert.glsl"
        "${SHADER_DIR}Uber3D.frag.glsl"
)

#
# Add executable target
#
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "Source/*.cxx"
)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_compile_definitions(${PROJECT_NAME} PUBLIC RR_ASSET_PATH=${RR_ASSET_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE RrFramework)
if (WIN32)
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()
file(GLOB_RECURSE RR_DEMO_BUILTIN_ASSETS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.exr"
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.glb"
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.vert.glsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/Asset/*.frag.glsl"
)
rr_embed_assets(
        ${PROJECT_NAME}
        "RrDemoBuiltinAssets"
        "DEMO_ASSET_"
        "DemoAssets"
        "${RR_DEMO_BUILTIN_ASSETS}")
