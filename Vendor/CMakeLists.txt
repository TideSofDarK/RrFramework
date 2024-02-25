find_package(Vulkan REQUIRED)

set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_FILE OFF CACHE BOOL "" FORCE)
set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
set(SDL_MISC OFF CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_3DNOW OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
CPMAddPackage("gh:libsdl-org/SDL#main")

CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#master")
CPMAddPackage("gh:zeux/volk#vulkan-sdk-1.3.275.0")

set(CGLM_STATIC ON CACHE BOOL "" FORCE)
CPMAddPackage("gh:recp/cglm#master")

set(IMGUI_TARGET "${PROJECT_NAME}ImGui")
add_library(${IMGUI_TARGET})
target_compile_definitions(${IMGUI_TARGET} PUBLIC "-DCIMGUI_USE_VULKAN=1" "-DCIMGUI_USE_SDL3=1" "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")
if (WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" __declspec\(dllexport\)")
else(WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" ")
endif(WIN32)
target_link_libraries(${IMGUI_TARGET} PRIVATE
        Vulkan::Headers
        SDL3::SDL3
)
target_include_directories(${IMGUI_TARGET} PUBLIC "./" PRIVATE "./imgui/")
target_sources (${IMGUI_TARGET} PRIVATE
    imgui/cimgui.cpp
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_sdl3.cpp
    imgui/imgui_impl_vulkan.cpp)

set(VENDOR_TARGET "${PROJECT_NAME}Vendor")
add_library(${VENDOR_TARGET} INTERFACE)
target_link_libraries(${VENDOR_TARGET} INTERFACE
        Vulkan::Headers
        volk::volk_headers
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        cglm
        ${IMGUI_TARGET}
)
target_compile_features(${VENDOR_TARGET} INTERFACE c_std_11 cxx_std_20)
set(VENDOR_TARGET "${PROJECT_NAME}Vendor" PARENT_SCOPE)
