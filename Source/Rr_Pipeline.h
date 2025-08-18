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

#include <Rr/Rr_Pipeline.h>

#include "Rr_Descriptor.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Platform.h>

typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;
struct Rr_DescriptorSetLayout
{
    Rr_BindingSet Set;
    VkDescriptorSetLayout Handle;
    uint32_t Hash;
};

struct Rr_PipelineLayout
{
    VkPipelineLayout Handle;
    size_t SetLayoutCount;
    Rr_DescriptorSetLayout *SetLayouts[RR_MAX_SETS];
    VkShaderStageFlags Stages[RR_MAX_SETS];
    atomic_uint RefCount;
};

#define RR_HIVE_TYPE      Rr_PipelineLayout
#define RR_HIVE_TYPE_NAME PipelineLayout
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

extern void Rr_DestroyPipelineLayout(Rr_PipelineLayout *PipelineLayout);

struct Rr_ComputePipeline
{
    VkPipeline Handle;
    Rr_PipelineLayout *Layout;
    atomic_uint RefCount;
};

#define RR_HIVE_TYPE      Rr_ComputePipeline
#define RR_HIVE_TYPE_NAME ComputePipeline
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

extern void Rr_DestroyComputePipeline(Rr_ComputePipeline *ComputePipeline);

struct Rr_GraphicsPipeline
{
    VkPipeline Handle;
    Rr_PipelineLayout *Layout;
    uint32_t ColorAttachmentCount;
    bool HasDepthStencil;
    atomic_uint RefCount;
};

#define RR_HIVE_TYPE      Rr_GraphicsPipeline
#define RR_HIVE_TYPE_NAME GraphicsPipeline
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

extern void Rr_DestroyGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipelin);

extern Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(Rr_BindingSet *Set);
