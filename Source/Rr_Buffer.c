#include "Rr_Buffer.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <SDL3/SDL_log.h>

Rr_Buffer *Rr_CreateBuffer(
    Rr_App *App,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    Rr_Bool CreateMapped)
{
    Rr_Buffer *Buffer = Rr_CreateObject(&App->ObjectStorage);

    VkBufferCreateInfo BufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = UsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VmaAllocationCreateInfo AllocationInfo = {
        .usage = MemoryUsage,
    };

    if (CreateMapped)
    {
        AllocationInfo.flags =
            VMA_ALLOCATION_CREATE_MAPPED_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(
        App->Renderer.Allocator,
        &BufferInfo,
        &AllocationInfo,
        &Buffer->Handle,
        &Buffer->Allocation,
        &Buffer->AllocationInfo);

    return Buffer;
}

Rr_Buffer *Rr_CreateDeviceVertexBuffer(Rr_App *App, size_t Size)
{
    Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        App,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        RR_FALSE);
}

Rr_Buffer *Rr_CreateDeviceUniformBuffer(Rr_App *App, size_t Size)
{
    // Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        App,
        Size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        RR_FALSE);
}

Rr_Buffer *Rr_CreateMappedBuffer(
    Rr_App *App,
    size_t Size,
    VkBufferUsageFlags UsageFlags)
{
    return Rr_CreateBuffer(
        App,
        Size,
        UsageFlags,
        VMA_MEMORY_USAGE_AUTO,
        RR_TRUE);
}

Rr_Buffer *Rr_CreateMappedVertexBuffer(Rr_App *App, size_t Size)
{
    return Rr_CreateMappedBuffer(App, Size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer)
{
    if (Buffer == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;

    vmaDestroyBuffer(Renderer->Allocator, Buffer->Handle, Buffer->Allocation);

    Rr_DestroyObject(&App->ObjectStorage, Buffer);
}

void Rr_UploadBufferAligned(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    void *Data,
    size_t DataLength,
    size_t Alignment)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_WriteBuffer *StagingBuffer = &UploadContext->StagingBuffer;
    if (StagingBuffer->Offset + DataLength >
        StagingBuffer->Buffer->AllocationInfo.size)
    {
        Rr_LogAbort(
            "Exceeding staging buffer size! Current offset is %zu, allocation "
            "size is %zu and total staging buffer size is %zu.",
            (size_t)StagingBuffer->Offset,
            DataLength,
            (size_t)StagingBuffer->Buffer->AllocationInfo.size);
    }

    VkCommandBuffer TransferCommandBuffer =
        UploadContext->TransferCommandBuffer;

    size_t BufferOffset = StagingBuffer->Offset;
    memcpy(
        (char *)StagingBuffer->Buffer->AllocationInfo.pMappedData +
            BufferOffset,
        Data,
        DataLength);
    if (Alignment == 0)
    {
        StagingBuffer->Offset += DataLength;
    }
    else
    {
        StagingBuffer->Offset += Rr_Align(BufferOffset + DataLength, Alignment);
    }

    vkCmdPipelineBarrier(
        TransferCommandBuffer,
        SrcStageMask,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        NULL,
        1,
        &(VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .buffer = Buffer,
            .offset = 0,
            .size = DataLength,
            .srcAccessMask = SrcAccessMask,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        },
        0,
        NULL);

    VkBufferCopy Copy = { .size = DataLength,
                          .srcOffset = BufferOffset,
                          .dstOffset = 0 };

    vkCmdCopyBuffer(
        TransferCommandBuffer,
        StagingBuffer->Buffer->Handle,
        Buffer,
        1,
        &Copy);

    if (!UploadContext->UseAcquireBarriers)
    {
        vkCmdPipelineBarrier(
            TransferCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            DstStageMask,
            0,
            0,
            NULL,
            1,
            &(VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = Buffer,
                .offset = 0,
                .size = DataLength,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = DstAccessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            },
            0,
            NULL);
    }
    else
    {
        UploadContext->ReleaseBarriers.BufferMemoryBarriers
            [UploadContext->AcquireBarriers.BufferMemoryBarrierCount] =
            (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = Buffer,
                .offset = 0,
                .size = DataLength,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };
        UploadContext->ReleaseBarriers.BufferMemoryBarrierCount++;

        UploadContext->AcquireBarriers.BufferMemoryBarriers
            [UploadContext->AcquireBarriers.BufferMemoryBarrierCount] =
            (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = Buffer,
                .offset = 0,
                .size = DataLength,
                .srcAccessMask = 0,
                .dstAccessMask = DstAccessMask,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };
        UploadContext->AcquireBarriers.BufferMemoryBarrierCount++;
    }
}

void Rr_UploadBuffer(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    void *Data,
    size_t DataLength)
{
    Rr_UploadBufferAligned(
        App,
        UploadContext,
        Buffer,
        SrcStageMask,
        SrcAccessMask,
        DstStageMask,
        DstAccessMask,
        Data,
        DataLength,
        0);
}

void Rr_UploadToDeviceBufferImmediate(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    void *Data,
    size_t Size)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
    Rr_Buffer *HostMappedBuffer =
        Rr_CreateMappedBuffer(App, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(HostMappedBuffer->AllocationInfo.pMappedData, Data, Size);
    VkBufferCopy BufferCopy = { .dstOffset = 0, .size = Size, .srcOffset = 0 };
    vkCmdCopyBuffer(
        CommandBuffer,
        HostMappedBuffer->Handle,
        DstBuffer->Handle,
        1,
        &BufferCopy);
    Rr_EndImmediate(Renderer);
    Rr_DestroyBuffer(App, HostMappedBuffer);
}

void Rr_UploadToUniformBuffer(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *DstBuffer,
    void *Data,
    size_t DataLength)
{
    Rr_UploadBufferAligned(
        App,
        UploadContext,
        DstBuffer->Handle,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_UNIFORM_READ_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_UNIFORM_READ_BIT,
        Data,
        DataLength,
        Rr_GetUniformAlignment(&App->Renderer));
}

void Rr_CopyToMappedUniformBuffer(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    void *Data,
    size_t Size,
    VkDeviceSize *DstOffset)
{
    uint32_t Alignment = App->Renderer.PhysicalDevice.Properties.properties
                             .limits.minUniformBufferOffsetAlignment;
    size_t AlignedSize = Rr_Align(Size, Alignment);
    if (*DstOffset + AlignedSize <= DstBuffer->AllocationInfo.size)
    {
        memcpy(
            (char *)DstBuffer->AllocationInfo.pMappedData + *DstOffset,
            Data,
            Size);
        *DstOffset += Size;
        *DstOffset = Rr_Align(*DstOffset, Alignment);
    }
    else
    {
        Rr_LogAbort(
            "Exceeding buffer size! Current offset is %zu, allocation size is "
            "%zu and total  buffer size is %zu.",
            *DstOffset,
            AlignedSize,
            DstBuffer->AllocationInfo.size);
    }
}
