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

#ifndef RR_BUFFER_H
#define RR_BUFFER_H

#include <Rr/Rr_Renderer.h>

static const uint64_t RR_WHOLE_SIZE = ~0ULL;

typedef struct Rr_Buffer Rr_Buffer;

typedef enum
{
    RR_BUFFER_FLAGS_UNIFORM_BIT = (1 << 0),
    RR_BUFFER_FLAGS_STORAGE_BIT = (1 << 1),
    RR_BUFFER_FLAGS_VERTEX_BIT = (1 << 2),
    RR_BUFFER_FLAGS_INDEX_BIT = (1 << 3),
    RR_BUFFER_FLAGS_INDIRECT_BIT = (1 << 4),
    RR_BUFFER_FLAGS_READBACK_BIT = (1 << 5),
    RR_BUFFER_FLAGS_STAGING_BIT = (1 << 6),
    RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT = (1 << 7),
    RR_BUFFER_FLAGS_MAPPED_BIT = (1 << 8),
    RR_BUFFER_FLAGS_PER_FRAME_BIT = (1 << 9),
} Rr_BufferFlagsBits;
typedef uint32_t Rr_BufferFlags;

RR_EXTERN Rr_Buffer *Rr_CreateBuffer(uint64_t Size, Rr_BufferFlags Flags);

RR_EXTERN void Rr_ReleaseBuffer(Rr_Buffer *Buffer);

RR_EXTERN void *Rr_GetMappedBufferData(Rr_Buffer *Buffer);

RR_EXTERN void *Rr_MapBuffer(Rr_Buffer *Buffer);

RR_EXTERN void Rr_UnmapBuffer(Rr_Buffer *Buffer);

RR_EXTERN void Rr_FlushBufferRange(
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint64_t Size);

#endif