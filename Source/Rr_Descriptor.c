/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Descriptor.h"

#include "Rr_Log.h"
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
            RR_ALLOC_NO_ZERO(gRenderer->Arena, sizeof(Rr_DescriptorPoolList));
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
    if (State->Layout != NULL)
    {
        for (size_t Index = 0; Index < RR_MAX_SETS; ++Index)
        {
            if (Index < State->Layout->SetLayoutCount)
            {
                VkDescriptorSetLayout OldLayout =
                    State->Layout->SetLayouts[Index]->Handle;
                VkDescriptorSetLayout NewLayout =
                    Layout->SetLayouts[Index]->Handle;
                if (OldLayout == NewLayout)
                {
                    continue;
                }
            }
            for (; Index < RR_MAX_SETS; ++Index)
            {
                State->Sets[Index] = VK_NULL_HANDLE;
            }
            break;
        }
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
        Scratch.Arena,
        sizeof(VkCopyDescriptorSet) * Layout->Key.TotalBindingCount);

    uint32_t CurrentCopyIndex = 0;

    /* TODO: It's slow and it's on the hot path. */

    for (uint32_t Index = 0; Index < RR_MAX_BINDINGS; ++Index)
    {
        Rr_PackedBinding *Binding = &Layout->Key.Bindings[Index];

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
            &State->Layout->SetLayouts[SetIndex]->Handle,
            &State->Sets[SetIndex]);

        if (OldSet)
        {
            Rr_Device *Device = State->Device;
            Rr_CopyDescriptorSet(
                State->Sets[SetIndex],
                OldSet,
                State->Device,
                State->Layout->SetLayouts[SetIndex]);
        }

        State->Dirty[SetIndex] = true;
    }

    return State->Sets[SetIndex];
}

#define RR_RETURN_IF_NO_LAYOUT(State)                                        \
    {                                                                        \
        if (!State->Layout)                                                  \
        {                                                                    \
            RR_LOG(                                                          \
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
    uint32_t Size,
    uint32_t Offset)
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
        if (State->Layout->SetLayouts[Index])
        {
            if (State->Dirty[Index])
            {
                uint32_t Count =
                    (uint32_t)State->Layout->SetLayoutCount - Index;

                Device->CmdBindDescriptorSets(
                    State->CommandBuffer,
                    BindPoint,
                    State->Layout->Handle,
                    Index,
                    Count,
                    &State->Sets[Index],
                    0,
                    NULL);

                break;
            }
        }
    }

    for (size_t Index = 0; Index < RR_MAX_SETS; ++Index)
    {
        State->Dirty[Index] = false;
    }
}
