find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SHADER_DIR "${PROJECT_SOURCE_DIR}/Asset/")
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS
        "${SHADER_DIR}*.frag" "${SHADER_DIR}*.vert" "${SHADER_DIR}*.comp"
)
set_source_files_properties(AssetDef.c PROPERTIES OBJECT_DEPENDS ${GLSL_SOURCE_FILES})

foreach (GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SHADER_DIR}/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_executable(${PROJECT_NAME} AssetDef.c Main.c App.c Rr/Rr.c Rr/RrAsset.c Rr/RrArray.c)
add_dependencies(${PROJECT_NAME} Shaders)
target_link_libraries(${PROJECT_NAME} PRIVATE ${VENDOR_TARGET})
if (WIN32)
    # if (CMAKE_BUILD_TYPE MATCHES Rel)
    #     set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
    # endif ()
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE On)
endif ()
add_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<CONFIG:DEBUG>:-DRR_DEBUG>")
target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /W3)
    endif ()
endif ()

