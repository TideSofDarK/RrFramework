/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "Rr_Descriptor.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_DESCRIPTOR
#include "Rr_LogMacro.h"
#include "Rr_Pipeline.h"
#include "Rr_Renderer.h"

#include <string.h>

Rr_DescriptorPoolList *Rr_AcquireDescriptorPoolList(void)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_DescriptorPoolList *Result = NULL;

    Rr_LockSpinlock(&gRenderer->DescriptorPoolListLock);

    if (gRenderer->DescriptorPoolList)
    {
        Result = gRenderer->DescriptorPoolList;
        gRenderer->DescriptorPoolList = Result->Next;

        Rr_UnlockSpinlock(&gRenderer->DescriptorPoolListLock);
    }
    else
    {
        Rr_UnlockSpinlock(&gRenderer->DescriptorPoolListLock);

        VkDescriptorPoolSize Sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RR_DESCRIPTOR_POOL_SIZE },
        };

        VkDescriptorPoolCreateInfo CreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = RR_DESCRIPTOR_POOL_SIZE,
            .poolSizeCount = RR_ARRAY_COUNT(Sizes),
            .pPoolSizes = Sizes,
        };

        VkDescriptorPool Pool;

        Device->CreateDescriptorPool(Device->Handle, &CreateInfo, NULL, &Pool);

        Rr_LockSpinlock(&gRenderer->Lock);
        Result =
            RR_ALLOC_NO_ZERO(sizeof(Rr_DescriptorPoolList), gRenderer->Arena);
        Rr_UnlockSpinlock(&gRenderer->Lock);

        Result->Handle = Pool;

        gRenderer->DescriptorPoolListCount++;
    }

    Result->Next = NULL;

    return Result;
}

void Rr_ReleaseDescriptorPoolList(Rr_DescriptorPoolList *List)
{
    if (List == NULL)
    {
        return;
    }

    Rr_Device *Device = &gRenderer->Device;

    Rr_DescriptorPoolList *First = List;

    while (List->Next)
    {
        Device->ResetDescriptorPool(Device->Handle, List->Handle, 0);
        List = List->Next;
    }

    Device->ResetDescriptorPool(Device->Handle, List->Handle, 0);

    Rr_LockSpinlock(&gRenderer->DescriptorPoolListLock);

    List->Next = gRenderer->DescriptorPoolList;
    gRenderer->DescriptorPoolList = First;

    Rr_UnlockSpinlock(&gRenderer->DescriptorPoolListLock);
}

void Rr_AllocateDescriptorSets(
    Rr_DescriptorPoolList *List,
    uint32_t Count,
    VkDescriptorSetLayout *Layouts,
    VkDescriptorSet *OutSets)
{
    Rr_Device *Device = &gRenderer->Device;

    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = List->Handle,
        .descriptorSetCount = Count,
        .pSetLayouts = Layouts,
    };
    VkResult Result =
        Device->AllocateDescriptorSets(Device->Handle, &AllocateInfo, OutSets);

    if (Result == VK_SUCCESS)
    {
        return;
    }

    /* TODO: Consider caching descriptor sets. */
    /* TODO: Consider iterating through "failed" pools as well. */

    Rr_DescriptorPoolList *NewList = Rr_AcquireDescriptorPoolList();
    List->Handle = NewList->Handle;
    NewList->Next = List->Next;
    List->Next = NewList;
    NewList->Handle = AllocateInfo.descriptorPool;

    AllocateInfo.descriptorPool = List->Handle;
    Result =
        Device->AllocateDescriptorSets(Device->Handle, &AllocateInfo, OutSets);

    assert(
        Result == VK_SUCCESS &&
        "Failed to allocate descriptor sets, too many descriptors requested?");
}

void Rr_InvalidateDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *Layout)
{
    size_t Index = 0;

    if (State->Layout != NULL)
    {
        for (; Index < RR_MAX_SETS; ++Index)
        {
            Rr_DescriptorSetLayout *OldLayout =
                State->Layout->Key.DescriptorSetLayouts[Index];
            Rr_DescriptorSetLayout *NewLayout =
                Layout->Key.DescriptorSetLayouts[Index];
            if (OldLayout != NewLayout)
            {
                break;
            }
        }
    }

    for (; Index < Layout->Key.DescriptorSetLayoutCount; ++Index)
    {
        Rr_DescriptorSetLayout *NewLayout =
            Layout->Key.DescriptorSetLayouts[Index];
        State->Sets[Index] =
            NewLayout->Key.BindingCount ? NULL : State->EmptyDescriptorSet;
    }

    State->Layout = Layout;
}

static inline void Rr_CopyDescriptorSet(
    VkDescriptorSet Dst,
    VkDescriptorSet Src,
    Rr_Device *Device,
    Rr_DescriptorSetLayout *Layout)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkCopyDescriptorSet *Copies = RR_ALLOC_NO_ZERO(
        sizeof(VkCopyDescriptorSet) * Layout->Key.BindingCount,
        Scratch.Arena);

    uint32_t CurrentCopyIndex = 0;

    for (uint32_t Index = 0; Index < Layout->Key.BindingCount; ++Index)
    {
        Rr_VulkanBinding *Binding = &Layout->Key.Bindings[Index];

        if (Binding->Count > 0)
        {
            Copies[CurrentCopyIndex++] = (VkCopyDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                .srcSet = Src,
                .srcBinding = Binding->Index,
                .srcArrayElement = 0,
                .dstSet = Dst,
                .dstBinding = Binding->Index,
                .dstArrayElement = 0,
                .descriptorCount = Binding->Count,
            };
        }
    }

    Device->UpdateDescriptorSets(
        Device->Handle,
        0,
        NULL,
        CurrentCopyIndex,
        Copies);

    Rr_DestroyScratch(Scratch);
}

static inline VkDescriptorSet Rr_GetDescriptorSet(
    Rr_DescriptorsState *State,
    uint32_t SetIndex)
{
    if (State->Sets[SetIndex] == VK_NULL_HANDLE || !State->Dirty[SetIndex])
    {
        VkDescriptorSet OldSet = State->Sets[SetIndex];

        Rr_AllocateDescriptorSets(
            State->DescriptorPoolList,
            1,
            &State->Layout->Key.DescriptorSetLayouts[SetIndex]->Handle,
            &State->Sets[SetIndex]);

        if (OldSet)
        {
            Rr_CopyDescriptorSet(
                State->Sets[SetIndex],
                OldSet,
                State->Device,
                State->Layout->Key.DescriptorSetLayouts[SetIndex]);
        }

        State->Dirty[SetIndex] = true;
    }

    return State->Sets[SetIndex];
}

#define RR_RETURN_IF_NO_LAYOUT(State)                                        \
    {                                                                        \
        if (!State->Layout)                                                  \
        {                                                                    \
            RR_LOG_ERROR(                                                    \
                "Attempting to bind a resource but current layout is NULL, " \
                "forgot to bind a pipeline?");                               \
            return;                                                          \
        }                                                                    \
    }

void Rr_WriteImageDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkImageView View,
    VkImageLayout Layout,
    VkSampler Sampler)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    VkDescriptorImageInfo ImageInfo = {
        .sampler = Sampler,
        .imageView = View,
        .imageLayout = Layout,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = Type,
        .pImageInfo = &ImageInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_WriteBufferDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkBuffer Handle,
    uint64_t Size,
    uint64_t Offset)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    VkDescriptorBufferInfo BufferInfo = {
        .buffer = Handle,
        .offset = Offset,
        .range = Size,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = Type,
        .pBufferInfo = &BufferInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_WriteSamplerDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkSampler Sampler)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    VkDescriptorImageInfo ImageInfo = {
        .sampler = Sampler,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &ImageInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    VkPipelineBindPoint BindPoint)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    for (uint32_t Index = 0; Index < RR_MAX_SETS; ++Index)
    {
        if (State->Dirty[Index])
        {
            Device->CmdBindDescriptorSets(
                State->CommandBuffer,
                BindPoint,
                State->Layout->Handle,
                Index,
                1,
                &State->Sets[Index],
                0,
                NULL);
            State->Dirty[Index] = false;
        }
    }
}
