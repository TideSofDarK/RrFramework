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

set(IMGUI_STATIC ON CACHE BOOL "" FORCE)
CPMAddPackage("gh:cimgui/cimgui#master")

add_library(${PROJECT_NAME}Vendor INTERFACE)
target_link_libraries(${PROJECT_NAME}Vendor INTERFACE
        Vulkan::Headers
        volk::volk_headers
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        cglm
        cimgui
)
target_compile_features(${PROJECT_NAME}Vendor INTERFACE c_std_11 cxx_std_20)
