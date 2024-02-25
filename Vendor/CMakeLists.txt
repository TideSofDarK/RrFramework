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

add_subdirectory("imgui")

set(VENDOR_TARGET "Vendor" CACHE INTERNAL "Project dependencies")
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
