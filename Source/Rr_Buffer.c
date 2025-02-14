#include "Rr_Buffer.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <assert.h>

Rr_Buffer *Rr_CreateBuffer_Internal(
    Rr_App *App,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool CreateMapped,
    bool Buffered)
{
    Rr_Buffer *Buffer = RR_GET_FREE_LIST_ITEM(&App->Renderer.Buffers, App->PermanentArena);
    Buffer->Usage = UsageFlags;

    /* Fixing VMA issues with small buffers. */
    Size = SDL_max(Size, 128);

    VkBufferCreateInfo BufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = UsageFlags,
    };

    VmaAllocationCreateInfo AllocationInfo = {
        .usage = MemoryUsage,
    };

    if(CreateMapped)
    {
        AllocationInfo.flags =
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    Buffer->AllocatedBufferCount = Buffered ? RR_FRAME_OVERLAP : 1;
    for(size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        vmaCreateBuffer(
            App->Renderer.Allocator,
            &BufferCreateInfo,
            &AllocationInfo,
            &AllocatedBuffer->Handle,
            &AllocatedBuffer->Allocation,
            &AllocatedBuffer->AllocationInfo);
    }

    return Buffer;
}

Rr_Buffer *Rr_CreateBuffer(Rr_App *App, size_t Size, Rr_BufferUsage Usage, bool Buffered)
{
    return Rr_CreateBuffer_Internal(
        App,
        Size,
        Rr_GetVulkanBufferUsage(Usage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        false,
        Buffered);
}

void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer)
{
    if(Buffer == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;

    for(size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        vmaDestroyBuffer(Renderer->Allocator, AllocatedBuffer->Handle, AllocatedBuffer->Allocation);
    }

    RR_RETURN_FREE_LIST_ITEM(&App->Renderer.Buffers, Buffer);
}

void Rr_UploadStagingBuffer(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkCommandBuffer CommandBuffer = UploadContext->CommandBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer = StagingBuffer->AllocatedBuffers;

    for(size_t AllocatedIndex = 0; AllocatedIndex < Buffer->AllocatedBufferCount; ++AllocatedIndex)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = Buffer->AllocatedBuffers + AllocatedIndex;

        vkCmdPipelineBarrier(
            CommandBuffer,
            SrcState.StageMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            NULL,
            1,
            &(VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = AllocatedBuffer->Handle,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcAccessMask = SrcState.AccessMask,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            },
            0,
            NULL);

        VkBufferCopy Copy = { .size = StagingSize, .srcOffset = StagingOffset, .dstOffset = 0 };

        vkCmdCopyBuffer(CommandBuffer, AllocatedStagingBuffer->Handle, AllocatedBuffer->Handle, 1, &Copy);

        if(!UploadContext->UseAcquireBarriers)
        {
            *RR_PUSH_SLICE(&UploadContext->ReleaseBufferMemoryBarriers, UploadContext->Arena) = (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = AllocatedBuffer->Handle,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };

            *RR_PUSH_SLICE(&UploadContext->AcquireBufferMemoryBarriers, UploadContext->Arena) = (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = AllocatedBuffer->Handle,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcAccessMask = 0,
                .dstAccessMask = DstState.AccessMask,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };
        }
        else
        {
            vkCmdPipelineBarrier(
                CommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                DstState.StageMask,
                0,
                0,
                NULL,
                1,
                &(VkBufferMemoryBarrier){
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .pNext = NULL,
                    .buffer = AllocatedBuffer->Handle,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = DstState.AccessMask,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                },
                0,
                NULL);
        }
    }
}

void Rr_UploadBuffer(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data)
{
    Rr_Buffer *StagingBuffer = Rr_CreateStagingBuffer(App, Data.Size);
    *RR_PUSH_SLICE(&UploadContext->StagingBuffers, UploadContext->Arena) = StagingBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer = StagingBuffer->AllocatedBuffers;
    memcpy(AllocatedStagingBuffer->AllocationInfo.pMappedData, Data.Pointer, Data.Size);

    Rr_UploadStagingBuffer(App, UploadContext, Buffer, SrcState, DstState, StagingBuffer, 0, Data.Size);
}

void Rr_UploadToDeviceBufferImmediate(Rr_App *App, Rr_Buffer *DstBuffer, Rr_Data Data)
{
    assert(DstBuffer != NULL);

    Rr_Renderer *Renderer = &App->Renderer;

    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);

    Rr_Buffer *SrcBuffer = Rr_CreateBuffer_Internal(
        App,
        Data.Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        true,
        false);
    Rr_AllocatedBuffer *SrcAllocatedBuffer = Rr_GetCurrentAllocatedBuffer(App, SrcBuffer);
    memcpy(SrcAllocatedBuffer->AllocationInfo.pMappedData, Data.Pointer, Data.Size);

    VkBufferCopy BufferCopy = { .dstOffset = 0, .size = Data.Size, .srcOffset = 0 };
    for(size_t Index = 0; Index < DstBuffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *DstAllocatedBuffer = &DstBuffer->AllocatedBuffers[Index];
        vkCmdCopyBuffer(CommandBuffer, SrcAllocatedBuffer->Handle, DstAllocatedBuffer->Handle, 1, &BufferCopy);
    }

    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(App, SrcBuffer);
}

Rr_Buffer *Rr_CreateStagingBuffer(Rr_App *App, size_t Size)
{
    return Rr_CreateBuffer_Internal(
        App,
        Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO,
        true,
        false);
}

Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_App *App, Rr_Buffer *Buffer)
{
    size_t AllocatedBufferIndex = App->Renderer.CurrentFrameIndex % Buffer->AllocatedBufferCount;
    return &Buffer->AllocatedBuffers[AllocatedBufferIndex];
}
