#pragma once

#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Math.h>
#include <Rr/Rr_Memory.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Platform.h>
#include <Rr/Rr_Renderer.h>

#include <vma/vk_mem_alloc.h>
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

typedef struct Rr_SyncState Rr_SyncState;
struct Rr_SyncState
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    union
    {
        VkImageLayout Layout;
    } Specific;
};

typedef struct Rr_BufferMemoryBarrier Rr_BufferMemoryBarrier;
struct Rr_BufferMemoryBarrier
{
    VkPipelineStageFlags SrcStageMask;
    VkPipelineStageFlags DstStageMask;
    VkAccessFlags SrcAccessMask;
    VkAccessFlags DstAccessMask;
    uint32_t SrcQueueFamilyIndex;
    uint32_t DstQueueFamilyIndex;
    VkBuffer Buffer;
    VkDeviceSize Offset;
    VkDeviceSize Size;
};

typedef struct Rr_ImageMemoryBarrier Rr_ImageMemoryBarrier;
struct Rr_ImageMemoryBarrier
{
    VkPipelineStageFlags SrcStageMask;
    VkPipelineStageFlags DstStageMask;
    VkAccessFlags SrcAccessMask;
    VkAccessFlags DstAccessMask;
    VkImageLayout OldLayout;
    VkImageLayout NewLayout;
    uint32_t SrcQueueFamilyIndex;
    uint32_t DstQueueFamilyIndex;
    VkImage Image;
    VkImageSubresourceRange SubresourceRange;
};

typedef struct Rr_BarrierBatch Rr_BarrierBatch;
struct Rr_BarrierBatch
{
    RR_SLICE(Rr_ImageMemoryBarrier) ImageBarriers;
    RR_SLICE(Rr_BufferMemoryBarrier) BufferBarriers;
    Rr_Map *VulkanHandleToBarrier;
};

typedef struct Rr_Queue Rr_Queue;
struct Rr_Queue
{
    VkCommandPool TransientCommandPool;
    VkQueue Handle;
    uint32_t FamilyIndex;
    Rr_SpinLock Lock;
};

typedef struct Rr_PhysicalDevice Rr_PhysicalDevice;
struct Rr_PhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceProperties2 Properties;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    VkPhysicalDeviceSubgroupProperties SubgroupProperties;
};

extern Rr_PhysicalDevice Rr_SelectPhysicalDevice(
    VkInstance Instance,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex,
    Rr_Arena *Arena);

extern void Rr_InitDeviceAndQueues(
    VkPhysicalDevice PhysicalDevice,
    uint32_t GraphicsQueueFamilyIndex,
    uint32_t TransferQueueFamilyIndex,
    VkDevice *OutDevice,
    VkQueue *OutGraphicsQueue,
    VkQueue *OutTransferQueue);

extern void Rr_BlitColorImage(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    VkImageAspectFlags AspectMask);

static inline VkExtent2D Rr_ToExtent2D(VkExtent3D *Extent)
{
    return (VkExtent2D){ .height = Extent->height, .width = Extent->width };
}

static inline VkExtent3D Rr_ToVulkanExtent3D(Rr_IntVec3 *Extent)
{
    return (VkExtent3D){ .width = Extent->Width, .height = Extent->Height, .depth = Extent->Depth };
}

static inline VkStencilOp Rr_GetVulkanStencilOp(Rr_StencilOp StencilOp)
{
    switch(StencilOp)
    {
        case RR_STENCIL_OP_KEEP:
            return VK_STENCIL_OP_KEEP;
        case RR_STENCIL_OP_ZERO:
            return VK_STENCIL_OP_ZERO;
        case RR_STENCIL_OP_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case RR_STENCIL_OP_INCREMENT_AND_CLAMP:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case RR_STENCIL_OP_DECREMENT_AND_CLAMP:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case RR_STENCIL_OP_INVERT:
            return VK_STENCIL_OP_INVERT;
        case RR_STENCIL_OP_INCREMENT_AND_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case RR_STENCIL_OP_DECREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:
            return 0;
    }
}

static inline VkShaderStageFlags Rr_GetVulkanShaderStageFlags(Rr_ShaderStage ShaderStage)
{
    VkShaderStageFlags ShaderStageFlags = 0;
    if((ShaderStage & RR_SHADER_STAGE_VERTEX_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if((ShaderStage & RR_SHADER_STAGE_FRAGMENT_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    return ShaderStageFlags;
}

static inline VkCompareOp Rr_GetVulkanCompareOp(Rr_CompareOp CompareOp)
{
    switch(CompareOp)
    {
        case RR_COMPARE_OP_NEVER:
            return VK_COMPARE_OP_NEVER;
        case RR_COMPARE_OP_LESS:
            return VK_COMPARE_OP_LESS;
        case RR_COMPARE_OP_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case RR_COMPARE_OP_LESS_OR_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RR_COMPARE_OP_GREATER:
            return VK_COMPARE_OP_GREATER;
        case RR_COMPARE_OP_NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case RR_COMPARE_OP_GREATER_OR_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RR_COMPARE_OP_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            return 0;
    }
}

static inline VkStencilOpState Rr_GetVulkanStencilOpState(Rr_StencilOpState State, Rr_DepthStencil *DepthStencil)
{
    return (VkStencilOpState){
        .compareOp = Rr_GetVulkanCompareOp(State.CompareOp),
        .failOp = Rr_GetVulkanStencilOp(State.FailOp),
        .passOp = Rr_GetVulkanStencilOp(State.PassOp),
        .depthFailOp = Rr_GetVulkanStencilOp(State.DepthFailOp),
        .writeMask = DepthStencil->WriteMask,
        .compareMask = DepthStencil->CompareMask,
        .reference = 0,
    };
}

static inline VkPolygonMode Rr_GetVulkanPolygonMode(Rr_PolygonMode Mode)
{
    switch(Mode)
    {
        case RR_POLYGON_MODE_LINE:
            return VK_POLYGON_MODE_LINE;
        default:
            return VK_POLYGON_MODE_FILL;
    }
}

static inline VkCullModeFlagBits Rr_GetVulkanCullMode(Rr_CullMode Mode)
{
    switch(Mode)
    {
        case RR_CULL_MODE_FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case RR_CULL_MODE_BACK:
            return VK_CULL_MODE_BACK_BIT;
        default:
            return VK_CULL_MODE_NONE;
    }
}

static inline VkFrontFace Rr_GetVulkanFrontFace(Rr_FrontFace FrontFace)
{
    switch(FrontFace)
    {
        case RR_FRONT_FACE_COUNTER_CLOCKWISE:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            return VK_FRONT_FACE_CLOCKWISE;
    }
}

static inline VkBlendFactor Rr_GetVulkanBlendFactor(Rr_BlendFactor BlendFactor)
{
    switch(BlendFactor)
    {
        case RR_BLEND_FACTOR_ZERO:
            return VK_BLEND_FACTOR_ZERO;
        case RR_BLEND_FACTOR_ONE:
            return VK_BLEND_FACTOR_ONE;
        case RR_BLEND_FACTOR_SRC_COLOR:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case RR_BLEND_FACTOR_DST_COLOR:
            return VK_BLEND_FACTOR_DST_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case RR_BLEND_FACTOR_SRC_ALPHA:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case RR_BLEND_FACTOR_DST_ALPHA:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case RR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case RR_BLEND_FACTOR_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case RR_BLEND_FACTOR_SRC_ALPHA_SATURATE:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default:
            return 0;
    }
}

static inline VkBlendOp Rr_GetVulkanBlendOp(Rr_BlendOp BlendOp)
{
    switch(BlendOp)
    {
        case RR_BLEND_OP_ADD:
            return VK_BLEND_OP_ADD;
        case RR_BLEND_OP_SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case RR_BLEND_OP_REVERSE_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RR_BLEND_OP_MIN:
            return VK_BLEND_OP_MIN;
        case RR_BLEND_OP_MAX:
            return VK_BLEND_OP_MAX;
        case RR_BLEND_OP_INVALID:
        default:
            return 0;
    }
}

static inline VkPrimitiveTopology Rr_GetVulkanTopology(Rr_Topology Topology)
{
    switch(Topology)
    {
        case RR_TOPOLOGY_TRIANGLE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case RR_TOPOLOGY_TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case RR_TOPOLOGY_LINE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RR_TOPOLOGY_LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case RR_TOPOLOGY_POINT_LIST:
        default:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

static inline VkFormat Rr_GetVulkanFormat(Rr_Format Format)
{
    switch(Format)
    {
        case RR_FORMAT_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RR_FORMAT_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_FORMAT_VEC2:
            return VK_FORMAT_R32G32_SFLOAT;
        case RR_FORMAT_VEC3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RR_FORMAT_VEC4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RR_FORMAT_NONE:
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

static inline size_t Rr_GetFormatSize(Rr_Format Format)
{
    switch(Format)
    {
        case RR_FORMAT_FLOAT:
            return sizeof(float);
        case RR_FORMAT_UINT:
            return sizeof(uint32_t);
        case RR_FORMAT_VEC2:
            return sizeof(float) * 2;
        case RR_FORMAT_VEC3:
            return sizeof(float) * 3;
        case RR_FORMAT_VEC4:
            return sizeof(float) * 4;
        case RR_FORMAT_NONE:
        default:
            return 0;
    }
}

static inline VkBorderColor Rr_GetVulkanBorderColor(Rr_BorderColor BorderColor)
{
    switch(BorderColor)
    {
        case RR_BORDER_COLOR_INT_TRANSPARENT_BLACK:
            return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        case RR_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case RR_BORDER_COLOR_INT_OPAQUE_BLACK:
            return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        case RR_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        case RR_BORDER_COLOR_INT_OPAQUE_WHITE:
            return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        default:
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
}

static inline VkSamplerAddressMode Rr_GetVulkanSamplerAddressMode(Rr_SamplerAddressMode SamplerAddressMode)
{
    switch(SamplerAddressMode)
    {
        case RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case RR_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static inline VkSamplerMipmapMode Rr_GetVulkanSamplerMipmapMode(Rr_SamplerMipmapMode SamplerMipmapMode)
{
    switch(SamplerMipmapMode)
    {
        case RR_SAMPLER_MIPMAP_MODE_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        default:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}

static inline VkFilter Rr_GetVulkanFilter(Rr_Filter Filter)
{
    switch(Filter)
    {
        case RR_FILTER_NEAREST:
            return VK_FILTER_NEAREST;
        default:
            return VK_FILTER_LINEAR;
    }
}

static VkBufferUsageFlags Rr_GetVulkanBufferUsage(Rr_BufferUsage Usage)
{
    VkBufferUsageFlags Result = 0;
    if((Usage & RR_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
    {
        Result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if((Usage & RR_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
    {
        Result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if((Usage & RR_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
    {
        Result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if((Usage & RR_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
    {
        Result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if((Usage & RR_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0)
    {
        Result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    return Result;
}

static Rr_TextureFormat Rr_GetTextureFormat(VkFormat TextureFormat)
{
    switch(TextureFormat)
    {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return RR_TEXTURE_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return RR_TEXTURE_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_D32_SFLOAT:
            return RR_TEXTURE_FORMAT_D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            return RR_TEXTURE_FORMAT_UNDEFINED;
    }
}

static VkFormat Rr_GetVulkanTextureFormat(Rr_TextureFormat TextureFormat)
{
    switch(TextureFormat)
    {
        case RR_TEXTURE_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RR_TEXTURE_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RR_TEXTURE_FORMAT_D32_SFLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}
