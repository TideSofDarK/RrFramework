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

#include <Rr/Rr_Pipeline.h>

#include "Rr_Descriptor.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Platform.h>

typedef struct Rr_PackedBinding Rr_PackedBinding;
struct Rr_PackedBinding
{
    uint16_t Index;
    uint16_t Type;
    uint16_t Stages;
    uint8_t Count;
    uint8_t ImageFormat;
};

typedef struct Rr_DescriptorSetLayoutKey Rr_DescriptorSetLayoutKey;
struct Rr_DescriptorSetLayoutKey
{
    uint32_t TotalBindingCount;
    Rr_PackedBinding Bindings[RR_MAX_BINDINGS];
};

typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;
struct Rr_DescriptorSetLayout
{
    Rr_DescriptorSetLayoutKey Key;
    VkDescriptorSetLayout Handle;
    Rr_DescriptorSetLayout *Children[4];
};

#define RR_HIVE_TYPE      Rr_DescriptorSetLayout
#define RR_HIVE_TYPE_NAME DescriptorSetLayout
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

typedef struct Rr_DescriptorSetLayoutStorage Rr_DescriptorSetLayoutStorage;
struct Rr_DescriptorSetLayoutStorage
{
    Rr_DescriptorSetLayout *Map;
    Rr_DescriptorSetLayoutHive Hive;
};

extern Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(
    Rr_DescriptorSetLayoutKey *Key);

struct Rr_PipelineLayout
{
    uint32_t SetLayoutCount;
    Rr_DescriptorSetLayout *SetLayouts[RR_MAX_SETS];

    VkPipelineLayout Handle;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_PipelineLayout
#define RR_HIVE_TYPE_NAME PipelineLayout
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyPipelineLayout(Rr_PipelineLayout *PipelineLayout);

struct Rr_ComputePipeline
{
    VkPipeline Handle;
    Rr_PipelineLayout *Layout;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_ComputePipeline
#define RR_HIVE_TYPE_NAME ComputePipeline
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyComputePipeline(Rr_ComputePipeline *ComputePipeline);

struct Rr_GraphicsPipeline
{
    VkPipeline Handle;
    Rr_PipelineLayout *Layout;
    uint32_t ColorAttachmentCount;
    bool HasDepthStencil;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_GraphicsPipeline
#define RR_HIVE_TYPE_NAME GraphicsPipeline
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipelin);
