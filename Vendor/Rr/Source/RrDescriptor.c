#include "RrDescriptor.h"

#include <SDL3/SDL.h>

#include "RrDefines.h"
#include "RrTypes.h"
#include "RrVulkan.h"
#include "RrArray.h"

VkDescriptorPool Rr_CreateDescriptorPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, size_t SetCount, Rr_DescriptorPoolSizeRatio* Ratios, size_t RatioCount)
{
    VkDescriptorPoolSize* PoolSizes = NULL;
    Rr_ArrayInit(PoolSizes, VkDescriptorPoolSize, RatioCount);
    for (size_t Index = 0; Index < RatioCount; Index++)
    {
        Rr_DescriptorPoolSizeRatio* Ratio = &Ratios[Index];
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

SDescriptorAllocator Rr_CreateDescriptorAllocator(VkDevice Device, size_t MaxSets, Rr_DescriptorPoolSizeRatio* Ratios, size_t RatioCount)
{
    SDescriptorAllocator DescriptorAllocator;
    Rr_ArrayInit(DescriptorAllocator.Ratios, Rr_DescriptorPoolSizeRatio, RatioCount);
    SDL_memcpy(DescriptorAllocator.Ratios, Ratios, RatioCount * sizeof(Rr_DescriptorPoolSizeRatio));

    VkDescriptorPool NewPool = Rr_CreateDescriptorPool(&DescriptorAllocator, Device, MaxSets, Ratios, RatioCount);
    Rr_ArrayInit(DescriptorAllocator.ReadyPools, VkDescriptorPool, 1);
    Rr_ArrayPush(DescriptorAllocator.ReadyPools, &NewPool);

    Rr_ArrayInit(DescriptorAllocator.FullPools, VkDescriptorPool, 1);

    DescriptorAllocator.SetsPerPool = (size_t)((f32)MaxSets * 1.5f);

    return DescriptorAllocator;
}

void Rr_ResetDescriptorAllocator(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
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
    Rr_ArrayEmpty(DescriptorAllocator->FullPools);
}

void Rr_DestroyDescriptorAllocator(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    size_t Count = Rr_ArrayCount(DescriptorAllocator->ReadyPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools[Index];
        vkDestroyDescriptorPool(Device, ReadyPool, NULL);
    }
    Rr_ArrayFree(DescriptorAllocator->ReadyPools);

    Count = Rr_ArrayCount(DescriptorAllocator->FullPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools[Index];
        vkDestroyDescriptorPool(Device, FullPool, NULL);
    }
    Rr_ArrayFree(DescriptorAllocator->FullPools);
    Rr_ArrayFree(DescriptorAllocator->Ratios);
}

VkDescriptorPool Rr_GetDescriptorPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
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
        NewPool = Rr_CreateDescriptorPool(
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

VkDescriptorSet Rr_AllocateDescriptorSet(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout)
{
    VkDescriptorPool Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);

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

        Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);
        AllocateInfo.descriptorPool = Pool;

        vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);
    }

    Rr_ArrayPush(DescriptorAllocator->ReadyPools, &Pool);
    return DescriptorSet;
}

Rr_DescriptorWriter Rr_CreateDescriptorWriter(size_t Images, size_t Buffers)
{
    Rr_DescriptorWriter Writer;
    Rr_ArrayInit(Writer.ImageInfos, VkDescriptorImageInfo, Images);
    Rr_ArrayInit(Writer.BufferInfos, VkDescriptorBufferInfo, Buffers);
    Rr_ArrayInit(Writer.Writes, VkWriteDescriptorSet, Images + Buffers);
    Rr_ArrayInit(Writer.Entries, SDescriptorWriterEntry, Images + Buffers);
    return Writer;
}

void Rr_WriteDescriptor_Image(Rr_DescriptorWriter* Writer, u32 Binding, VkImageView View, VkSampler Sampler, VkImageLayout Layout, VkDescriptorType Type)
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

void Rr_WriteDescriptor_Buffer(Rr_DescriptorWriter* Writer, u32 Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type)
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

void Rr_ResetDescriptorWriter(Rr_DescriptorWriter* Writer)
{
    Rr_ArrayEmpty(Writer->ImageInfos);
    Rr_ArrayEmpty(Writer->BufferInfos);
    Rr_ArrayEmpty(Writer->Writes);
    Rr_ArrayEmpty(Writer->Entries);
}

void Rr_DestroyDescriptorWriter(Rr_DescriptorWriter* Writer)
{
    Rr_ArrayFree(Writer->ImageInfos);
    Rr_ArrayFree(Writer->BufferInfos);
    Rr_ArrayFree(Writer->Writes);
    Rr_ArrayFree(Writer->Entries);
}

void Rr_UpdateDescriptorSet(Rr_DescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set)
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

void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type)
{
    if (Builder->Count >= RR_MAX_LAYOUT_BINDINGS)
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

void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder* Builder)
{
    *Builder = (Rr_DescriptorLayoutBuilder){ 0 };
}

VkDescriptorSetLayout Rr_BuildDescriptorLayout(Rr_DescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags)
{
    for (u32 Index = 0; Index < RR_MAX_LAYOUT_BINDINGS; ++Index)
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
    vkCreateDescriptorSetLayout(Device, &Info, NULL, &Set);

    return Set;
}
