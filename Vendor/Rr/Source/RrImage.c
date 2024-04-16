#include "RrImage.h"
#include "RrArray.h"
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_JPEG
#define STBI_NO_GIF
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STBI_NO_FAILURE_STRINGS
// #define STBI_MALLOC Rr_Malloc
// #define STBI_REALLOC Rr_Realloc
// #define STBI_FREE Rr_Free
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#include <tinyexr/tinyexr.h>

#include "RrRenderer.h"
#include "RrAsset.h"
#include "RrTypes.h"
#include "RrHelpers.h"
#include "RrVulkan.h"
#include "RrBuffer.h"
#include "RrLib.h"
#include "RrMemory.h"

static Rr_Image Rr_CreateImage_Internal(Rr_Renderer* const Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, VmaAllocationCreateInfo AllocationCreateInfo, b8 bMipMapped)
{
    Rr_Image Image = { 0 };
    Image.Format = Format;
    Image.Extent = Extent;

    VkImageCreateInfo Info = GetImageCreateInfo(Image.Format, Usage, Image.Extent);

    if (bMipMapped)
    {
        Info.mipLevels = (u32)(SDL_floor(SDL_log(SDL_max(Extent.width, Extent.height)))) + 1;
    }

    VkResult Result = vmaCreateImage(Renderer->Allocator, &Info, &AllocationCreateInfo, &Image.Handle, &Image.Allocation, NULL);

    VkImageAspectFlags AspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Format == RR_DEPTH_FORMAT)
    {
        AspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo ViewInfo = GetImageViewCreateInfo(Image.Format, Image.Handle, AspectFlag);
    ViewInfo.subresourceRange.levelCount = Info.mipLevels;

    vkCreateImageView(Renderer->Device, &ViewInfo, NULL, &Image.View);

    return Image;
}

Rr_Image Rr_CreateImage(Rr_Renderer* const Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped)
{
    VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    return Rr_CreateImage_Internal(Renderer, Extent, Format, Usage, AllocationCreateInfo, bMipMapped);
}

Rr_Image Rr_CreateImageFromPNG(Rr_Asset* Asset, Rr_Renderer* const Renderer, VkImageUsageFlags Usage, b8 bMipMapped, VkImageLayout InitialLayout)
{
    const i32 DesiredChannels = 4;
    i32 Channels;
    VkExtent3D Extent = { .depth = 1 };
    stbi_uc* ParsedImage = stbi_load_from_memory((stbi_uc*)Asset->Data, (i32)Asset->Length, (i32*)&Extent.width, (i32*)&Extent.height, &Channels, DesiredChannels);

    size_t DataSize = Extent.width * Extent.height * DesiredChannels;

    Rr_Buffer Buffer = Rr_CreateBuffer(Renderer->Allocator, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, ParsedImage, DataSize);
    stbi_image_free(ParsedImage);

    Rr_Image Image = Rr_CreateImage(Renderer, Extent, RR_COLOR_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, bMipMapped);

    Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier Transition = {
        .Image = Image.Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .AccessMask = VK_ACCESS_2_NONE,
        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    };
    Rr_ChainImageBarrier(&Transition, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy Copy = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = Extent,
    };

    vkCmdCopyBufferToImage(Renderer->ImmediateMode.CommandBuffer, Buffer.Handle, Image.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

    Rr_ChainImageBarrier(&Transition, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT, InitialLayout);
    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(&Buffer, Renderer->Allocator);

    return Image;
}

Rr_Image Rr_CreateImageFromEXR(Rr_Asset* Asset, Rr_Renderer* const Renderer)
{
    VkImageUsageFlags Usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    int Result;
    const char* Error;

    EXRVersion Version;
    Result = ParseEXRVersionFromMemory(&Version, (const unsigned char*)Asset->Data, Asset->Length);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file!");
        abort();
    }

    EXRHeader Header;
    Result = ParseEXRHeaderFromMemory(&Header, &Version, (const unsigned char*)Asset->Data, Asset->Length, &Error);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file: %s", Error);
        FreeEXRErrorMessage(Error);
        abort();
    }

    EXRImage Image;
    InitEXRImage(&Image);

    Result = LoadEXRImageFromMemory(&Image, &Header, (const unsigned char*)Asset->Data, Asset->Length, &Error);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file: %s", Error);
        FreeEXRHeader(&Header);
        FreeEXRErrorMessage(Error);
        abort();
    }

    /* Normalize depth. */
    const float Near = 0.1f;
    const float Far = 100.0f;
    float MinDepth = FLT_MAX;
    float MaxDepth = FLT_MIN;
    for (int Index = 0; Index < Image.width * Image.height; Index++)
    {
        float* const Current = ((float*)Image.images[0]) + Index;
        if (*Current > Far)
        {
//            *Current = 1.0f;
            continue;
        }
        MinDepth = SDL_min(*Current, MinDepth);
        MaxDepth = SDL_max(*Current, MaxDepth);
//         *Current = 1.0f - ((*Current - Near) / (Far - Near));
        // if (*Current >= Far)
        // {
        //     *Current = 1.0f;
        // }
        // if (*Current <= Near)
        // {
        //     *Current = Near;
        // }
//         *Current = ((2.0f * Near) / *Current - Far - Near / (Far - Near)) * -1.0f;

//         *Current = (2.0f * Near) / (Far + Near - *Current * (Far - Near));
        // *Current /= 99.9f;
        //
        // *Current = *Current / 99.9f;
        // *Current = 1.0f - (*Current - 1.0f);
        // *Current = (float)rand() / (float)(RAND_MAX / 1.0f);
        // *Current = SDL_clamp(*Current, 0.0f, 1.0f);
        // *Current = *Current / 99.9f;
        // fprintf(stderr, "%f\n", *Current);
        // *Current = (2.0f * Near) / (Far + Near - *Current * (Far - Near));
    }

    for (int Index = 0; Index < Image.width * Image.height; Index++)
    {
        float* const Current = ((float*)Image.images[0]) + Index;
        if (*Current > Far)
        {
            *Current = 1.0f;
            continue;
        }
        const float shit = *Current;
        *Current =(-(((Near + Far) * shit - (2.0f * Near)) / ((Near - Far) * shit)));

        //        *Current = *Current / MaxDepth;
//        MinDepth = SDL_min(*Current, MinDepth);
//        MaxDepth = SDL_max(*Current, MaxDepth);
//        *Current =  *Current / (MaxDepth) * (MaxDepth - MinDepth);
//        *Current = ((*Current) / (MaxDepth - MinDepth));
        // if (*Current >= Far)
        // {
        //     *Current = 1.0f;
        // }
        // if (*Current <= Near)
        // {
        //     *Current = Near;
        // }
        //         *Current = ((2.0f * Near) / *Current - Far - Near / (Far - Near)) * -1.0f;
//        *Current = (*(((float*)Image.images[0]) + Index) * *(((float*)Image.images[1]) + Index) * *(((float*)Image.images[2]) + Index)) / 3.0f;
//                 *Current = 1.0f - ( 1.0f+ (2.0f * Near) / (Far + Near - *Current * (Far - Near)));
//                         *Current = 1.0 + (2.0f * MinDepth) / (MaxDepth + MinDepth - *Current * (MaxDepth - MinDepth));
        // *Current /= 99.9f;
        //
        // *Current = *Current / 99.9f;
        // *Current = 1.0f - (*Current - 1.0f);
        // *Current = (float)rand() / (float)(RAND_MAX / 1.0f);
        // *Current = SDL_clamp(*Current, 0.0f, 1.0f);
        // *Current = *Current / 99.9f;
        // fprintf(stderr, "%f\n", *Current);
        // *Current = (2.0f * Near) / (Far + Near - *Current * (Far - Near));
    }

    for (int Index = 0; Index < Image.width; Index++)
    {
        float* const Current0 = ((float*)Image.images[0]) + Index;
//        float* const Current1 = ((float*)Image.images[1]) + Index;
//        float* const Current2 = ((float*)Image.images[2]) + Index;
        fprintf(stderr, "%f\n", *Current0);
    }

    VkExtent3D Extent = { .width = Image.width, .height = Image.height, .depth = 1 };

    size_t DataSize = Extent.width * Extent.height * sizeof(f32);

    Rr_Buffer Buffer = Rr_CreateBuffer(Renderer->Allocator, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, Image.images[0], DataSize);

    Rr_Image DepthImage = Rr_CreateImage(Renderer, Extent, RR_PRERENDERED_DEPTH_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier Transition = {
        .Image = DepthImage.Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .AccessMask = VK_ACCESS_2_NONE,
        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    };
    Rr_ChainImageBarrier_Aspect(
        &Transition,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkBufferImageCopy Copy = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = Extent,
    };

    vkCmdCopyBufferToImage(Renderer->ImmediateMode.CommandBuffer, Buffer.Handle, DepthImage.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

    Rr_ChainImageBarrier_Aspect(
        &Transition,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(&Buffer, Renderer->Allocator);

    FreeEXRHeader(&Header);
    FreeEXRImage(&Image);

    return DepthImage;
}

static void Rr_SaveDepthToFile(Rr_Renderer* Renderer, Rr_Buffer* HostMappedBuffer, size_t Width, size_t Height)
{
    EXRHeader Header;
    InitEXRHeader(&Header);

    EXRImage Image;
    InitEXRImage(&Image);

    Image.num_channels = 1;

    float* Data;
    Rr_ArrayInit(Data, float, HostMappedBuffer->AllocationInfo.size);
    SDL_memcpy(Data, HostMappedBuffer->AllocationInfo.pMappedData, HostMappedBuffer->AllocationInfo.size);
    for (int Index = 0; Index < Width; Index++)
    {
//         float Near = 0.1f;
//         float Far = 100.0f;
//         Data[Index] = (2.0f * Near) / (Far + Near - Data[Index] * (Far - Near));
        fprintf(stderr, "%f\n", Data[Index]);
    }
    // u8** images = (u8**)&HostMappedBuffer->AllocationInfo.pMappedData;

    u8* Images[1] = { (u8*)Data };

    Image.images = Images;
    Image.width = (int)Width;
    Image.height = (int)Height;

    Header.num_channels = 1;
    Header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * Header.num_channels);
    strncpy(Header.channels[0].name, "R", 255);
    Header.channels[0].name[strlen("R")] = '\0';

    Header.pixel_types = (int*)malloc(sizeof(int) * Header.num_channels);
    Header.requested_pixel_types = (int*)malloc(sizeof(int) * Header.num_channels);
    Header.pixel_types[0] = TINYEXR_PIXELTYPE_FLOAT;
    Header.requested_pixel_types[0] = TINYEXR_PIXELTYPE_FLOAT;

    const char* Error = NULL;
    int ret = SaveEXRImageToFile(&Image, &Header, "depth.exr", &Error);
    if (ret != TINYEXR_SUCCESS)
    {
        fprintf(stderr, "Save EXR err: %s\n", Error);
        FreeEXRErrorMessage(Error);
        abort();
    }
    printf("Saved exr file. [ %s ] \n", "depth.exr");

    free(Header.channels);
    free(Header.pixel_types);
    free(Header.requested_pixel_types);

    Rr_ArrayFree(Data);
}

void Rr_CopyImageToHost(Rr_Renderer* Renderer, Rr_Image* Image)
{
    VmaAllocationInfo AllocationInfo;
    vmaGetAllocationInfo(Renderer->Allocator, Image->Allocation, &AllocationInfo);

    Rr_Buffer Buffer = Rr_CreateMappedBuffer(Renderer->Allocator, AllocationInfo.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier SrcTransition = {
        .Image = Image->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .AccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    };
    Rr_ChainImageBarrier_Aspect(&SrcTransition, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkBufferImageCopy Copy = {
        .imageExtent = Image->Extent,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdCopyImageToBuffer(
        Renderer->ImmediateMode.CommandBuffer,
        Image->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        Buffer.Handle,
        1,
        &Copy);

    Rr_EndImmediate(Renderer);

    // for (int Index = Image->Extent.width * Image->Extent.height / 2; Index < Image->Extent.width * (Image->Extent.height / 2 + 1); ++Index)
    // {
    //     fprintf(stderr, "%f\n", ((float*)Buffer.AllocationInfo.pMappedData)[Index]);
    // }

    Rr_SaveDepthToFile(Renderer, &Buffer, Image->Extent.width, Image->Extent.height);

    Rr_DestroyBuffer(&Buffer, Renderer->Allocator);
}

void Rr_DestroyImage(Rr_Renderer* const Renderer, Rr_Image* AllocatedImage)
{
    if (AllocatedImage->Handle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyImageView(Renderer->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Renderer->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);
}
