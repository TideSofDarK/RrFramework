#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Load.h"

struct Rr_UploadContext;

typedef struct Rr_Buffer Rr_Buffer;
struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

extern Rr_Buffer *
Rr_CreateBuffer(
    Rr_App *App,
    Rr_USize Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    Rr_Bool bHostMapped);

extern Rr_Buffer *
Rr_CreateDeviceVertexBuffer(Rr_App *App, Rr_USize Size);

extern Rr_Buffer *
Rr_CreateDeviceUniformBuffer(Rr_App *App, Rr_USize Size);

extern Rr_Buffer *
Rr_CreateMappedBuffer(
    Rr_App *App,
    Rr_USize Size,
    VkBufferUsageFlags UsageFlags);

extern Rr_Buffer *
Rr_CreateMappedVertexBuffer(Rr_App *App, Rr_USize Size);

extern void
Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer);

typedef struct Rr_WriteBuffer Rr_WriteBuffer;
struct Rr_WriteBuffer
{
    Rr_Buffer *Buffer;
    VkDeviceSize Offset;
};

extern void
Rr_UploadBufferAligned(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    void *Data,
    Rr_USize DataLength,
    Rr_USize Alignment);

extern void
Rr_UploadBuffer(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    void *Data,
    Rr_USize DataLength);

extern void
Rr_UploadToDeviceBufferImmediate(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    void *Data,
    Rr_USize Size);

extern void
Rr_UploadToUniformBuffer(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_Buffer *DstBuffer,
    void *Data,
    Rr_USize DataLength);

extern void
Rr_CopyToMappedUniformBuffer(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    void *Data,
    Rr_USize Size,
    VkDeviceSize *DstOffset);
