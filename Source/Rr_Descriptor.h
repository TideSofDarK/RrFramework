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

#pragma once

#include "Rr_Vulkan.h"

#include <Rr/Rr_Memory.h>
#include <Rr/Rr_Pipeline.h>

#define RR_MAX_BINDINGS         16
#define RR_MAX_SETS             4
#define RR_DESCRIPTOR_POOL_SIZE 128

typedef struct Rr_DescriptorPoolList Rr_DescriptorPoolList;
struct Rr_DescriptorPoolList
{
    VkDescriptorPool Handle;
    Rr_DescriptorPoolList *Next;
};

extern Rr_DescriptorPoolList *Rr_AcquireDescriptorPoolList(void);

extern void Rr_AllocateDescriptorSets(
    Rr_DescriptorPoolList **ListRef,
    uint32_t Count,
    VkDescriptorSetLayout *Layouts,
    VkDescriptorSet *OutSets);

extern void Rr_ResetDescriptorPools(Rr_DescriptorPoolList *List);

typedef struct Rr_DescriptorsState Rr_DescriptorsState;
struct Rr_DescriptorsState
{
    Rr_Device *Device;
    VkCommandBuffer CommandBuffer;
    Rr_DescriptorPoolList **ListRef;
    Rr_PipelineLayout *Layout;
    VkDescriptorSet Sets[RR_MAX_SETS];
    bool Dirty[RR_MAX_SETS];
};

extern void Rr_InvalidateDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *Layout);

extern void Rr_WriteImageDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkImageView View,
    VkImageLayout Layout,
    VkSampler Sampler);

extern void Rr_WriteBufferDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkBuffer Handle,
    uint32_t Size,
    uint32_t Offset);

extern void Rr_WriteSamplerDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkSampler Sampler);

extern void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    VkPipelineBindPoint BindPoint);
