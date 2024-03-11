#pragma once

#include <stdlib.h>

#include <SDL_log.h>
#include <SDL_assert.h>
#include <SDL_rwops.h>
#include <SDL_filesystem.h>

#include "RrVulkan.h"
#include "RrTypes.h"
#include "RrHelpers.h"

/* ==============
 * Shader Loading
 * ============== */

static b32 LoadShaderModule(const char* Filename, VkDevice Device, VkShaderModule* OutShaderModule)
{
    char* BasePath = SDL_GetBasePath();
    size_t BasePathLength = SDL_strlen(BasePath);
    size_t ShaderPathLength = BasePathLength + SDL_strlen(Filename) + 1;
    char* ShaderPath = SDL_stack_alloc(char, ShaderPathLength);
    SDL_strlcpy(ShaderPath, BasePath, ShaderPathLength);
    SDL_strlcpy(ShaderPath + BasePathLength, Filename, ShaderPathLength - BasePathLength);
    SDL_free(BasePath);

    SDL_RWops* File = SDL_RWFromFile(ShaderPath, "rb");

    if (!File)
    {
        goto ShaderError;
    }

    SDL_RWseek(File, 0, SDL_RW_SEEK_END);
    i64 FileSize = SDL_RWtell(File);
    SDL_RWseek(File, 0, SDL_RW_SEEK_SET);

    size_t BufferSize = (u32)FileSize / sizeof(u32);
    u32* Buffer = SDL_stack_alloc(u32, BufferSize);

    SDL_RWread(File, Buffer, FileSize);
    SDL_RWclose(File);

    VkShaderModuleCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .codeSize = BufferSize * sizeof(u32),
        .pCode = Buffer,
    };

    VkShaderModule ShaderModule;
    VkResult Result = vkCreateShaderModule(Device, &CreateInfo, NULL, &ShaderModule);
    SDL_stack_free(Buffer);
    SDL_stack_free(ShaderPath);

    if (Result != VK_SUCCESS)
    {
        goto ShaderError;
    }

    *OutShaderModule = ShaderModule;

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Loaded shader: %s", ShaderPath);

    return true;

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

/* ============================
 * Rr_DescriptorLayoutBuilder API
 * ============================ */

static void DescriptorLayoutBuilder_Add(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type)
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

static void DescriptorLayoutBuilder_Clear(Rr_DescriptorLayoutBuilder* Builder)
{
    *Builder = (Rr_DescriptorLayoutBuilder){ 0 };
}

static VkDescriptorSetLayout DescriptorLayoutBuilder_Build(Rr_DescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags)
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
 * Rr_TransitionImage API
 * ==================== */

static void TransitionImage_To(
    Rr_TransitionImage* TransitionImage,
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
