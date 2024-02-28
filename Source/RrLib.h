#pragma once

#include <stdlib.h>
#include <stdio.h>

#include "RrTypes.h"

#include <SDL_log.h>
#include <SDL_filesystem.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#define VK_ASSERT(Expr)                                                                                                              \
    {                                                                                                                                \
        VkResult Result = Expr;                                                                                                      \
        if (Result != VK_SUCCESS)                                                                                                    \
        {                                                                                                                            \
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Assertion " #Expr " == VK_SUCCESS failed! Result is %s", string_VkResult(Result)); \
            abort();                                                                                                                 \
        }                                                                                                                            \
    }

/* =======================
 * Struct Creation Helpers
 * ======================= */

static VkPipelineShaderStageCreateInfo GetShaderStageInfo(VkShaderStageFlagBits Stage, VkShaderModule Module)
{
    VkPipelineShaderStageCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .pName = "main",
        .stage = Stage,
        .module = Module
    };

    return Info;
}

static VkRenderingAttachmentInfo GetRenderingAttachmentInfo(VkImageView View, VkClearValue* InClearValue, VkImageLayout Layout)
{
    VkRenderingAttachmentInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = View,
        .imageLayout = Layout,
        .loadOp = InClearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = InClearValue ? *InClearValue : (VkClearValue){ 0 }
    };

    return Info;
}

static VkRenderingInfo GetRenderingInfo(VkExtent2D RenderExtent, VkRenderingAttachmentInfo* ColorAttachment,
    VkRenderingAttachmentInfo* DepthAttachment)
{
    VkRenderingInfo Info = { 0 };
    Info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    Info.pNext = NULL;

    Info.renderArea = (VkRect2D){ .offset = (VkOffset2D){ 0, 0 }, .extent = RenderExtent };
    Info.layerCount = 1;
    Info.colorAttachmentCount = 1;
    Info.pColorAttachments = ColorAttachment;
    Info.pDepthAttachment = DepthAttachment;
    Info.pStencilAttachment = NULL;

    return Info;
}

static VkImageCreateInfo GetImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent)
{
    VkImageCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = Format,
        .extent = Extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = UsageFlags,
    };
    return Info;
}

static VkImageViewCreateInfo GetImageViewCreateInfo(VkFormat Format, VkImage Image, VkImageAspectFlags AspectFlags)
{
    VkImageViewCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .image = Image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = Format,
        .subresourceRange = {
            .aspectMask = AspectFlags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    return Info;
}

static VkFenceCreateInfo GetFenceCreateInfo(VkFenceCreateFlags Flags)
{
    VkFenceCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static VkSemaphoreCreateInfo GetSemaphoreCreateInfo(VkSemaphoreCreateFlags Flags)
{
    VkSemaphoreCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static VkCommandBufferBeginInfo GetCommandBufferBeginInfo(VkCommandBufferUsageFlags Flags)
{
    VkCommandBufferBeginInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = Flags,
        .pInheritanceInfo = NULL,
    };
    return Info;
}

static VkImageSubresourceRange GetImageSubresourceRange(VkImageAspectFlags AspectMask)
{
    VkImageSubresourceRange SubImage = {
        .aspectMask = AspectMask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    return SubImage;
}

static VkSemaphoreSubmitInfo GetSemaphoreSubmitInfo(VkPipelineStageFlags2 StageMask, VkSemaphore Semaphore)
{
    VkSemaphoreSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
        .semaphore = Semaphore,
        .value = 1,
        .stageMask = StageMask,
        .deviceIndex = 0,
    };

    return Info;
}

static VkCommandBufferSubmitInfo GetCommandBufferSubmitInfo(VkCommandBuffer CommandBuffer)
{
    VkCommandBufferSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = NULL,
        .commandBuffer = CommandBuffer,
        .deviceMask = 0,
    };

    return Info;
}

static VkCommandBufferAllocateInfo GetCommandBufferAllocateInfo(VkCommandPool CommandPool, u32 Count)
{
    VkCommandBufferAllocateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = Count,
    };

    return Info;
}

static VkSubmitInfo2 GetSubmitInfo(VkCommandBufferSubmitInfo* CommandBufferSubInfoPtr, VkSemaphoreSubmitInfo* SignalSemaphoreInfo,
    VkSemaphoreSubmitInfo* WaitSemaphoreInfo)
{
    VkSubmitInfo2 Info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = NULL,
        .waitSemaphoreInfoCount = WaitSemaphoreInfo == NULL ? 0u : 1u,
        .pWaitSemaphoreInfos = WaitSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = CommandBufferSubInfoPtr,
        .signalSemaphoreInfoCount = SignalSemaphoreInfo == NULL ? 0u : 1u,
        .pSignalSemaphoreInfos = SignalSemaphoreInfo,
    };

    return Info;
}

static SPipelineBuilder GetPipelineBuilder(void)
{
    SPipelineBuilder PipelineBuilder = {
        .InputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO },
        .Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO },
        .ColorBlendAttachment = { 0 },
        .Multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0f,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE },
        .PipelineLayout = VK_NULL_HANDLE,
        .DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
        .RenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO }
    };

    return PipelineBuilder;
}

/* ==============
 * Shader Loading
 * ============== */

static b32 LoadShaderModule(const char* Filename, VkDevice Device, VkShaderModule* OutShaderModule)
{
    char* BasePath = SDL_GetBasePath();
    size_t BasePathLength = strlen(BasePath);
    char* ShaderPath = StackAlloc(char, BasePathLength + strlen(Filename) + 1);
    strcpy(ShaderPath, BasePath);
    strcpy(ShaderPath + BasePathLength, Filename);
    SDL_free(BasePath);

    FILE* File = fopen(ShaderPath, "r");

    if (!File)
    {
        goto ShaderError;
    }

    fseek(File, 0L, SEEK_END);
    size_t FileSize = (size_t)ftell(File);

    size_t BufferSize = FileSize / sizeof(u32);
    u32* Buffer = StackAlloc(u32, BufferSize);

    rewind(File);
    fread(Buffer, sizeof(Buffer[0]), FileSize, File);
    fclose(File);

    VkShaderModuleCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .codeSize = BufferSize * sizeof(u32),
        .pCode = Buffer,
    };

    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &CreateInfo, NULL, &ShaderModule) != VK_SUCCESS)
    {
        goto ShaderError;
    }

    *OutShaderModule = ShaderModule;

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Loaded shader: %s", ShaderPath);

    return TRUE;

ShaderError:
    SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Error when building the shader! Path: %s", ShaderPath);
    abort();
}

/* ==========
 * Operations
 * ========== */

static void CopyImageToImage(VkCommandBuffer CommandBuffer, VkImage Source, VkImage Destination, VkExtent2D SrcSize, VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,

        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .srcOffsets = { { 0 }, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },

        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .dstOffsets = { { 0 }, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = Source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = Destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &BlitRegion,
        .filter = VK_FILTER_LINEAR,
    };

    vkCmdBlitImage2(CommandBuffer, &BlitInfo);
}

/* ====================
 * SPipelineBuilder API
 * ==================== */

static void PipelineBuilder_Default(SPipelineBuilder* PipelineBuilder, VkShaderModule VertModule, VkShaderModule FragModule, VkFormat ColorFormat, VkFormat DepthFormat, VkPipelineLayout Layout)
{
    PipelineBuilder->PipelineLayout = Layout;

    PipelineBuilder->ShaderStages[0] = GetShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, VertModule);
    PipelineBuilder->ShaderStages[1] = GetShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragModule);
    PipelineBuilder->ShaderStageCount = 2;

    PipelineBuilder->InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PipelineBuilder->InputAssembly.primitiveRestartEnable = VK_FALSE;

    PipelineBuilder->Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    PipelineBuilder->Rasterizer.lineWidth = 1.0f;
    PipelineBuilder->Rasterizer.cullMode = VK_CULL_MODE_NONE;
    PipelineBuilder->Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    PipelineBuilder->ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    PipelineBuilder->ColorBlendAttachment.blendEnable = VK_FALSE;

    PipelineBuilder->ColorAttachmentFormat = ColorFormat;
    PipelineBuilder->RenderInfo.pColorAttachmentFormats = &PipelineBuilder->ColorAttachmentFormat;
    PipelineBuilder->RenderInfo.colorAttachmentCount = 1;
    PipelineBuilder->RenderInfo.depthAttachmentFormat = DepthFormat;

    PipelineBuilder->DepthStencil.depthTestEnable = VK_FALSE;
    PipelineBuilder->DepthStencil.depthWriteEnable = VK_FALSE;
    PipelineBuilder->DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    PipelineBuilder->DepthStencil.depthBoundsTestEnable = VK_FALSE;
    PipelineBuilder->DepthStencil.stencilTestEnable = VK_FALSE;
    PipelineBuilder->DepthStencil.front = (VkStencilOpState){ 0 };
    PipelineBuilder->DepthStencil.back = (VkStencilOpState){ 0 };
    PipelineBuilder->DepthStencil.minDepthBounds = 0.0f;
    PipelineBuilder->DepthStencil.maxDepthBounds = 1.0f;
}

/* ============================
 * SDescriptorLayoutBuilder API
 * ============================ */

static void DescriptorLayoutBuilder_Add(SDescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type)
{
    if (Builder->Count >= MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = (VkDescriptorSetLayoutBinding){
        .binding = Binding,
        .descriptorType = Type,
        .descriptorCount = 1,
    };
    Builder->Count++;
}

static void DescriptorLayoutBuilder_Clear(SDescriptorLayoutBuilder* Builder)
{
    *Builder = (SDescriptorLayoutBuilder){ 0 };
}

static VkDescriptorSetLayout DescriptorLayoutBuilder_Build(SDescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags)
{
    for (u32 Index = 0; Index < MAX_LAYOUT_BINDINGS; ++Index)
    {
        VkDescriptorSetLayoutBinding* Binding = &Builder->Bindings[Index];
        Binding->stageFlags |= ShaderStageFlags;
    }

    VkDescriptorSetLayoutCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = Builder->Count,
        .pBindings = Builder->Bindings,
    };

    VkDescriptorSetLayout Set;
    VK_ASSERT(vkCreateDescriptorSetLayout(Device, &Info, NULL, &Set));

    return Set;
}

/* ====================
 * SDescriptorAllocator
 * ==================== */

static void DescriptorAllocator_Init(SDescriptorAllocator* DescriptorAllocator, VkDevice device, u32 maxSets, SDescriptorPoolSizeRatio* poolRatios, u32 PoolRatioCount)
{
    VkDescriptorPoolSize* PoolSizes = StackAlloc(VkDescriptorPoolSize, PoolRatioCount);
    for (u32 Index = 0; Index < PoolRatioCount; --Index)
    {
        PoolSizes[Index] = (VkDescriptorPoolSize){
            .type = poolRatios[Index].Type,
            .descriptorCount = (u32)(poolRatios[Index].Ratio * (f32)maxSets),
        };
    }

    VkDescriptorPoolCreateInfo PoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = maxSets,
        .poolSizeCount = PoolRatioCount,
        .pPoolSizes = PoolSizes,
    };

    VK_ASSERT(vkCreateDescriptorPool(device, &PoolCreateInfo, NULL, &DescriptorAllocator->Pool));
}

static void DescriptorAllocator_ClearDescriptors(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    VK_ASSERT(vkResetDescriptorPool(Device, DescriptorAllocator->Pool, 0));
}

static void DescriptorAllocator_DestroyPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    vkDestroyDescriptorPool(Device, DescriptorAllocator->Pool, NULL);
}

static VkDescriptorSet DescriptorAllocator_Allocate(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = DescriptorAllocator->Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet DescriptorSet;
    VK_ASSERT(vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet));

    return DescriptorSet;
}

/* ====================
 * STransitionImage API
 * ==================== */

static void TransitionImage_To(
    STransitionImage* TransitionImage,
    VkPipelineStageFlags2 DstStageMask,
    VkAccessFlags2 DstAccessMask,
    VkImageLayout NewLayout)
{
    VkImageMemoryBarrier2 ImageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        .pNext = NULL,

        .srcStageMask = TransitionImage->StageMask,
        .srcAccessMask = TransitionImage->AccessMask,
        .dstStageMask = DstStageMask,
        .dstAccessMask = DstAccessMask,

        .oldLayout = TransitionImage->Layout,
        .newLayout = NewLayout,

        .image = TransitionImage->Image,
        .subresourceRange = GetImageSubresourceRange((NewLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
    };

    VkDependencyInfo DepInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &ImageBarrier,
    };

    vkCmdPipelineBarrier2(TransitionImage->CommandBuffer, &DepInfo);

    TransitionImage->Layout = NewLayout;
    TransitionImage->StageMask = DstStageMask;
    TransitionImage->AccessMask = DstAccessMask;
}

/* ====================
 * SAllocatedBuffer API
 * ==================== */

static void AllocatedBuffer_Init(SAllocatedBuffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped)
{
    VkBufferCreateInfo BufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = UsageFlags,
    };

    VmaAllocationCreateInfo AllocationInfo = {
        .usage = MemoryUsage,
    };

    if (bHostMapped)
    {
        AllocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VK_ASSERT(vmaCreateBuffer(Allocator, &BufferInfo, &AllocationInfo, &Buffer->Handle, &Buffer->Allocation, &Buffer->AllocationInfo))
}

static void AllocatedBuffer_Cleanup(SAllocatedBuffer* Buffer, VmaAllocator Allocator)
{
    vmaDestroyBuffer(Allocator, Buffer->Handle, Buffer->Allocation);
}

/* =======
 * SRr API
 * ======= */

static VkCommandBuffer Rr_BeginImmediate(SRr* Rr)
{
    SImmediateMode* ImmediateMode = &Rr->ImmediateMode;
    VK_ASSERT(vkResetFences(Rr->Device, 1, &ImmediateMode->Fence))
    VK_ASSERT(vkResetCommandBuffer(ImmediateMode->CommandBuffer, 0))

    VkCommandBufferBeginInfo BeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_ASSERT(vkBeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo))
}

static void Rr_EndImmediate(SRr* Rr)
{
    SImmediateMode* ImmediateMode = &Rr->ImmediateMode;

    VK_ASSERT(vkEndCommandBuffer(ImmediateMode->CommandBuffer))

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(ImmediateMode->CommandBuffer);
    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, NULL, NULL);

    VK_ASSERT(vkQueueSubmit2(Rr->GraphicsQueue.Handle, 1, &SubmitInfo, ImmediateMode->Fence))
    VK_ASSERT(vkWaitForFences(Rr->Device, 1, &ImmediateMode->Fence, true, UINT64_MAX))
}

static void Rr_UploadMesh(SRr* Rr, SMeshBuffers* MeshBuffers, MeshIndexType* Indices, size_t IndexCount, SVertex* Vertices, size_t VertexCount)
{
    size_t VertexBufferSize = sizeof(SVertex) * VertexCount;
    size_t IndexBufferSize = sizeof(MeshIndexType) * IndexCount;

    AllocatedBuffer_Init(
        &MeshBuffers->VertexBuffer,
        Rr->Allocator,
        VertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO,
        false);

    VkBufferDeviceAddressInfo DeviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = MeshBuffers->VertexBuffer.Handle
    };
    MeshBuffers->VertexBufferAddress = vkGetBufferDeviceAddress(Rr->Device, &DeviceAddressInfo);

    AllocatedBuffer_Init(
        &MeshBuffers->IndexBuffer,
        Rr->Allocator,
        IndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO, false);

    SAllocatedBuffer StagingBuffer = { 0 };
    AllocatedBuffer_Init(
        &StagingBuffer,
        Rr->Allocator,
        VertexBufferSize + IndexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO, true);

    void* StagingData = StagingBuffer.AllocationInfo.pMappedData;

    memcpy((char*)StagingData, (void*)Vertices, VertexBufferSize);
    memcpy((char*)StagingData + VertexBufferSize, (void*)Indices, IndexBufferSize);

    Rr_BeginImmediate(Rr);

    VkBufferCopy VertexCopy = {
        .dstOffset = 0,
        .srcOffset = 0,
        .size = VertexBufferSize
    };

    vkCmdCopyBuffer(Rr->ImmediateMode.CommandBuffer, StagingBuffer.Handle, MeshBuffers->VertexBuffer.Handle, 1, &VertexCopy);

    VkBufferCopy IndexCopy = {
        .dstOffset = 0,
        .srcOffset = VertexBufferSize,
        .size = IndexBufferSize
    };

    vkCmdCopyBuffer(Rr->ImmediateMode.CommandBuffer, StagingBuffer.Handle, MeshBuffers->IndexBuffer.Handle, 1, &IndexCopy);

    Rr_EndImmediate(Rr);

    AllocatedBuffer_Cleanup(&StagingBuffer, Rr->Allocator);
}

static void Rr_CleanupMesh(SRr* const Rr, SMeshBuffers* Mesh)
{
    AllocatedBuffer_Cleanup(&Mesh->IndexBuffer, Rr->Allocator);
    AllocatedBuffer_Cleanup(&Mesh->VertexBuffer, Rr->Allocator);
}

static SFrameData* Rr_GetCurrentFrame(SRr* Rr)
{
    return &Rr->Frames[Rr->FrameNumber % FRAME_OVERLAP];
}

static VkPipeline Rr_BuildPipeline(SRr* Rr, SPipelineBuilder* PipelineBuilder)
{
    VkPipelineViewportStateCreateInfo ViewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &PipelineBuilder->ColorBlendAttachment
    };

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL
    };

    VkDynamicState DynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .pDynamicStates = DynamicState,
        .dynamicStateCount = ArraySize(DynamicState)
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &PipelineBuilder->RenderInfo,
        .stageCount = PipelineBuilder->ShaderStageCount,
        .pStages = PipelineBuilder->ShaderStages,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &PipelineBuilder->InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &PipelineBuilder->Rasterizer,
        .pMultisampleState = &PipelineBuilder->Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &PipelineBuilder->DepthStencil,
        .layout = PipelineBuilder->PipelineLayout,
        .pDynamicState = &DynamicStateInfo
    };

    VkPipeline Pipeline;
    VK_ASSERT(vkCreateGraphicsPipelines(Rr->Device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &Pipeline))

    return Pipeline;
}

static void Rr_DestroyAllocatedImage(SRr* Rr, SAllocatedImage* AllocatedImage)
{
    if (AllocatedImage->Handle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyImageView(Rr->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Rr->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);
}
