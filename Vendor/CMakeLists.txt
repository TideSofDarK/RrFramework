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
CPMAddPackage("gh:zeux/volk#vulkan-sdk-1.3.280.0")

set(CGLM_STATIC ON CACHE BOOL "" FORCE)
CPMAddPackage("gh:recp/cglm#master")

add_subdirectory("imgui")

set(VENDOR_TARGET "Vendor" CACHE INTERNAL "RetroRenderer dependencies")
add_library(${VENDOR_TARGET} STATIC)
target_compile_definitions(${VENDOR_TARGET} PUBLIC
        TINYEXR_USE_MINIZ=0
        TINYEXR_USE_STB_ZLIB=1
)
target_compile_definitions(${VENDOR_TARGET} PRIVATE
        STBI_NO_STDIO
        STBI_NO_JPEG
        STBI_NO_GIF
        STBI_NO_BMP
        STBI_NO_PSD
        STBI_NO_PIC
        STBI_NO_PNM
        STBI_NO_HDR
        STBI_NO_TGA
        STBI_NO_FAILURE_STRINGS
)

target_sources(${VENDOR_TARGET} PRIVATE Vendor.cpp cJSON/cJSON.c)
target_link_libraries(${VENDOR_TARGET} PUBLIC
        Vulkan::Headers
        volk::volk_headers
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        cglm
        ${IMGUI_TARGET}
)
