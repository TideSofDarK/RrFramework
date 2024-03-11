#include "RrDescriptor.h"

#include <SDL3/SDL.h>

#include "RrVulkan.h"
#include "RrArray.h"

VkDescriptorPool DescriptorAllocator_CreatePool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, size_t SetCount, SDescriptorPoolSizeRatio* Ratios, size_t RatioCount)
{
    VkDescriptorPoolSize* PoolSizes = NULL;
    Rr_ArrayInit(PoolSizes, VkDescriptorPoolSize, RatioCount);
    for (size_t Index = 0; Index < RatioCount; Index++)
    {
        SDescriptorPoolSizeRatio* Ratio = &Ratios[Index];
        Rr_ArrayPush(PoolSizes, &((VkDescriptorPoolSize){ .type = Ratio->Type, .descriptorCount = (u32)(Ratio->Ratio * (f32)SetCount) }));
    }

    VkDescriptorPoolCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = SetCount,
        .poolSizeCount = RatioCount,
        .pPoolSizes = PoolSizes
    };

    VkDescriptorPool NewPool;
    vkCreateDescriptorPool(Device, &Info, NULL, &NewPool);
    return NewPool;
}

void DescriptorAllocator_Init(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, size_t MaxSets, SDescriptorPoolSizeRatio* Ratios, size_t RatioCount)
{
    Rr_ArrayInit(DescriptorAllocator->Ratios, SDescriptorPoolSizeRatio, RatioCount);
    SDL_memcpy(DescriptorAllocator->Ratios, Ratios, RatioCount * sizeof(SDescriptorPoolSizeRatio));

    VkDescriptorPool NewPool = DescriptorAllocator_CreatePool(DescriptorAllocator, Device, MaxSets, Ratios, RatioCount);
    Rr_ArrayInit(DescriptorAllocator->ReadyPools, VkDescriptorPool, 1);
    Rr_ArrayPush(DescriptorAllocator->ReadyPools, &NewPool);

    Rr_ArrayInit(DescriptorAllocator->FullPools, VkDescriptorPool, 1);

    DescriptorAllocator->SetsPerPool = (size_t)((f32)MaxSets * 1.5f);
}

void DescriptorAllocator_ClearPools(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    size_t Count = Rr_ArrayCount(DescriptorAllocator->ReadyPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools[Index];
        vkResetDescriptorPool(Device, ReadyPool, 0);
    }

    Count = Rr_ArrayCount(DescriptorAllocator->FullPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools[Index];
        vkResetDescriptorPool(Device, FullPool, 0);

        Rr_ArrayPush(DescriptorAllocator->ReadyPools, &FullPool);
    }
    Rr_ArrayEmpty(DescriptorAllocator->FullPools, false);
}

void DescriptorAllocator_Cleanup(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    size_t Count = Rr_ArrayCount(DescriptorAllocator->ReadyPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools[Index];
        vkDestroyDescriptorPool(Device, ReadyPool, NULL);
    }
    Rr_ArrayEmpty(DescriptorAllocator->ReadyPools, true);

    Count = Rr_ArrayCount(DescriptorAllocator->FullPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools[Index];
        vkDestroyDescriptorPool(Device, FullPool, NULL);
    }
    Rr_ArrayEmpty(DescriptorAllocator->FullPools, true);
    Rr_ArrayEmpty(DescriptorAllocator->Ratios, true);
}

VkDescriptorPool DescriptorAllocator_GetPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    VkDescriptorPool NewPool;
    size_t ReadyCount = Rr_ArrayCount(DescriptorAllocator->ReadyPools);
    if (ReadyCount != 0)
    {
        NewPool = DescriptorAllocator->ReadyPools[ReadyCount - 1];
        Rr_ArrayPop(DescriptorAllocator->ReadyPools);
    }
    else
    {
        NewPool = DescriptorAllocator_CreatePool(
            DescriptorAllocator,
            Device,
            DescriptorAllocator->SetsPerPool,
            DescriptorAllocator->Ratios,
            Rr_ArrayCount(DescriptorAllocator->Ratios));

        DescriptorAllocator->SetsPerPool = (size_t)((float)DescriptorAllocator->SetsPerPool * 1.5f);

        if (DescriptorAllocator->SetsPerPool > 4096)
        {
            DescriptorAllocator->SetsPerPool = 4096;
        }
    }

    return NewPool;
}

VkDescriptorSet DescriptorAllocator_Allocate(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout)
{
    VkDescriptorPool Pool = DescriptorAllocator_GetPool(DescriptorAllocator, Device);

    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &Layout
    };

    VkDescriptorSet DescriptorSet;
    VkResult Result = vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);

    if (Result == VK_ERROR_OUT_OF_POOL_MEMORY || Result == VK_ERROR_FRAGMENTED_POOL)
    {
        Rr_ArrayPush(DescriptorAllocator->FullPools, &Pool);

        Pool = DescriptorAllocator_GetPool(DescriptorAllocator, Device);
        AllocateInfo.descriptorPool = Pool;

        VK_ASSERT(vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet));
    }

    Rr_ArrayPush(DescriptorAllocator->ReadyPools, &Pool);
    return DescriptorSet;
}

void DescriptorWriter_Init(SDescriptorWriter* Writer, size_t Images, size_t Buffers)
{
    Rr_ArrayInit(Writer->ImageInfos, VkDescriptorImageInfo, Images);
    Rr_ArrayInit(Writer->BufferInfos, VkDescriptorBufferInfo, Buffers);
    Rr_ArrayInit(Writer->Writes, VkWriteDescriptorSet, Images + Buffers);
    Rr_ArrayInit(Writer->Entries, SDescriptorWriterEntry, Images + Buffers);
}

void DescriptorWriter_WriteImage(SDescriptorWriter* Writer, u32 Binding, VkImageView View, VkSampler Sampler, VkImageLayout Layout, VkDescriptorType Type)
{
    Rr_ArrayPush(Writer->ImageInfos, &((VkDescriptorImageInfo){ .sampler = Sampler, .imageView = View, .imageLayout = Layout }));

    Rr_ArrayPush(Writer->Writes, &((VkWriteDescriptorSet){
                                     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstBinding = Binding,
                                     .dstSet = VK_NULL_HANDLE,
                                     .descriptorCount = 1,
                                     .descriptorType = Type,
                                 }));

    Rr_ArrayPush(Writer->Entries, &((SDescriptorWriterEntry){ .Type = EDescriptorWriterEntryType_Image, .Index = Rr_ArrayCount(Writer->ImageInfos) - 1 }));
}

void DescriptorWriter_WriteBuffer(SDescriptorWriter* Writer, u32 Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type)
{
    Rr_ArrayPush(Writer->BufferInfos, &((VkDescriptorBufferInfo){ .range = Size, .buffer = Buffer, .offset = Offset }));

    Rr_ArrayPush(Writer->Writes, &((VkWriteDescriptorSet){
                                     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstBinding = Binding,
                                     .dstSet = VK_NULL_HANDLE,
                                     .descriptorCount = 1,
                                     .descriptorType = Type,
                                 }));

    Rr_ArrayPush(Writer->Entries, &((SDescriptorWriterEntry){ .Type = EDescriptorWriterEntryType_Buffer, .Index = Rr_ArrayCount(Writer->BufferInfos) - 1 }));
}

void DescriptorWriter_Cleanup(SDescriptorWriter* Writer)
{
    Rr_ArrayEmpty(Writer->ImageInfos, true);
    Rr_ArrayEmpty(Writer->BufferInfos, true);
    Rr_ArrayEmpty(Writer->Writes, true);
    Rr_ArrayEmpty(Writer->Entries, true);
}

void DescriptorWriter_Update(SDescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set)
{
    size_t WritesCount = Rr_ArrayCount(Writer->Writes);
    for (size_t Index = 0; Index < WritesCount; ++Index)
    {
        SDescriptorWriterEntry* Entry = &Writer->Entries[Index];
        VkWriteDescriptorSet* Write = &Writer->Writes[Index];
        Write->dstSet = Set;
        switch (Entry->Type)
        {
            case EDescriptorWriterEntryType_Buffer:
            {
                Write->pBufferInfo = &Writer->BufferInfos[Entry->Index];
            }
            break;
            case EDescriptorWriterEntryType_Image:
            {
                Write->pImageInfo = &Writer->ImageInfos[Entry->Index];
            }
            break;
            default:
            {
            }
            break;
        }
    }

    vkUpdateDescriptorSets(Device, (u32)WritesCount, Writer->Writes, 0, NULL);
}
