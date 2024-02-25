set(IMGUI_TARGET "imgui" CACHE INTERNAL "Dear ImGui library")
add_library(${IMGUI_TARGET})
target_compile_definitions(${IMGUI_TARGET} PUBLIC "-DCIMGUI_USE_VULKAN=1" "-DCIMGUI_USE_SDL3=1" PRIVATE "-DVK_NO_PROTOTYPES=1" "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")
if (WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" __declspec\(dllexport\)")
else(WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" ")
endif(WIN32)
target_link_libraries(${IMGUI_TARGET} PRIVATE
        Vulkan::Headers
        SDL3::SDL3
)
target_include_directories(${IMGUI_TARGET} PUBLIC "../" PRIVATE "./")
target_sources (${IMGUI_TARGET} PRIVATE
    cimgui.cpp
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_impl_sdl3.cpp
    imgui_impl_vulkan.cpp)
