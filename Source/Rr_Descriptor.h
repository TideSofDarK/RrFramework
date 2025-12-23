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

#pragma once

#include "Rr_Vulkan.h"

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

extern void Rr_ReleaseDescriptorPoolList(Rr_DescriptorPoolList *List);

extern void Rr_AllocateDescriptorSets(
    Rr_DescriptorPoolList *List,
    uint32_t Count,
    VkDescriptorSetLayout *Layouts,
    VkDescriptorSet *OutSets);

typedef struct Rr_DescriptorsState Rr_DescriptorsState;
struct Rr_DescriptorsState
{
    Rr_Device *Device;
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet EmptyDescriptorSet;
    Rr_DescriptorPoolList *DescriptorPoolList;
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
    uint64_t Size,
    uint64_t Offset);

extern void Rr_WriteSamplerDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkSampler Sampler);

extern void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    VkPipelineBindPoint BindPoint);
