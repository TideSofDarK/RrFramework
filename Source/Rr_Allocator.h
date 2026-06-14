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

struct Rr_Buffer;
struct Rr_AllocatedBuffer;
struct Rr_Image;
struct Rr_AllocatedImage;

#define RR_MINIMAL_ALLOCATION 256
#define RR_BIG_CHUNK_SIZE     RR_MIBIBYTES(256)
#define RR_SMALL_CHUNK_SIZE   RR_MIBIBYTES(64)

typedef struct Rr_Range Rr_Range;
struct Rr_Range
{
    VkDeviceSize Offset;
    VkDeviceSize Size;
    VkDeviceSize AlignedOffset;
    bool Free;

    Rr_Range *Previous;
    Rr_Range *Next;
    Rr_Range *PreviousFree;
    Rr_Range *NextFree;
};

#define RR_HIVE_TYPE               Rr_Range
#define RR_HIVE_TYPE_NAME          Range
#define RR_HIVE_PREFIX             Rr_
#define RR_HIVE_MIN_BLOCK_CAPACITY 64
#include "Rr_Hive.h"

typedef struct Rr_Chunk Rr_Chunk;
struct Rr_Chunk
{
    VkDeviceSize Size;
    VkDeviceMemory Memory;
    void *MappedData;
    Rr_AtomicInt SoftAllocations;
    bool Dedicated;

    Rr_Range *FirstRange;
    Rr_Range *FirstFreeRange;

    Rr_Chunk *Next;
};

#define RR_HIVE_TYPE               Rr_Chunk
#define RR_HIVE_TYPE_NAME          Chunk
#define RR_HIVE_PREFIX             Rr_
#define RR_HIVE_MIN_BLOCK_CAPACITY 64
#include "Rr_Hive.h"

typedef struct Rr_MemoryType Rr_MemoryType;
struct Rr_MemoryType
{
    Rr_Chunk *FirstChunk;
    VkDeviceSize HeapSize;
    VkDeviceSize ChunkSize;
    bool DeviceLocal;
    bool HostVisible;
};

typedef struct Rr_Allocator Rr_Allocator;
struct Rr_Allocator
{
    VkDeviceSize BufferImageGranularity;
    VkDeviceSize NonCoherentAtomSize;
    VkDeviceSize BigChunkSize;
    VkDeviceSize SmallChunkSize;
    uint32_t MemoryTypeCount;
    Rr_MemoryType *MemoryTypes;
    Rr_RangeHive RangeHive;
    Rr_ChunkHive ChunkHive;
    Rr_AtomicInt HardAllocations;
    Rr_AtomicInt SoftAllocations;
    Rr_Spinlock Lock;
};

extern void Rr_InitAllocator(
    Rr_Allocator *Allocator,
    Rr_PhysicalDevice *PhysicalDevice);

extern void Rr_CleanupAllocator(Rr_Allocator *Allocator);

extern bool Rr_AllocBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_Buffer *Buffer);

extern void Rr_FreeBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_Buffer *Buffer);

extern void Rr_FlushBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_AllocatedBuffer *AllocatedBuffer,
    size_t Offset,
    size_t Size);

extern bool Rr_AllocImageMemory(
    Rr_Allocator *Allocator,
    struct Rr_Image *Image);

extern void Rr_FreeImageMemory(Rr_Allocator *Allocator, struct Rr_Image *Image);
