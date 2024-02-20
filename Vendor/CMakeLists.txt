CPMAddPackage("gh:libsdl-org/SDL#main")

CPMAddPackage("gh:KhronosGroup/Vulkan-Headers#vulkan-sdk-1.3.268.0")
CPMAddPackage("gh:KhronosGroup/SPIRV-Headers#vulkan-sdk-1.3.268.0")
CPMAddPackage("gh:KhronosGroup/SPIRV-Tools#vulkan-sdk-1.3.268.0")
CPMAddPackage("gh:KhronosGroup/glslang#vulkan-sdk-1.3.268.0")
CPMAddPackage("gh:zeux/volk#vulkan-sdk-1.3.268.0")

CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#master")
CPMAddPackage("gh:recp/cglm#master")
CPMAddPackage("gh:cimgui/cimgui#master")

add_library(Vendor INTERFACE)
target_link_libraries(Vendor INTERFACE
        Vulkan::Headers
        SPIRV-Headers::SPIRV-Headers
        volk::volk_headers
        glslang
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        cglm
        cimgui
)
