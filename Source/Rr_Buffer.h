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

#include <Rr/Rr_Buffer.h>

#include "Rr_UploadContext.h"
#include "Rr_Vulkan.h"

typedef struct Rr_AllocatedBuffer Rr_AllocatedBuffer;
struct Rr_AllocatedBuffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

struct Rr_Buffer
{
    Rr_BufferFlags Flags;
    VkBufferUsageFlags Usage;
    size_t AllocatedBufferCount;
    Rr_AllocatedBuffer AllocatedBuffers[RR_MAX_FRAME_OVERLAP];
};

extern void Rr_UploadStagingBuffer(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize);

extern void Rr_UploadBuffer(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data);

extern Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(
    Rr_Renderer *Renderer,
    Rr_Buffer *Buffer);
