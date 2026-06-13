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

typedef struct Rr_Buffer Rr_Buffer;

typedef enum
{
    RR_BUFFER_FLAGS_UNIFORM_BIT = 1U << 0,
    RR_BUFFER_FLAGS_STORAGE_BIT = 1U << 1,
    RR_BUFFER_FLAGS_VERTEX_BIT = 1U << 2,
    RR_BUFFER_FLAGS_INDEX_BIT = 1U << 3,
    RR_BUFFER_FLAGS_INDIRECT_BIT = 1U << 4,
    RR_BUFFER_FLAGS_MAPPED_BIT = 1U << 5,
    RR_BUFFER_FLAGS_PER_FRAME_BIT = 1U << 6,
    RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT = 1U << 7,
    RR_BUFFER_FLAGS_STAGING_BIT = 1U << 8,
    RR_BUFFER_FLAGS_READBACK_BIT = 1U << 9,
} Rr_BufferFlagsBits;
typedef uint32_t Rr_BufferFlags;

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_Buffer *RR_CC Rr_CreateBuffer(uint64_t Size, Rr_BufferFlags Flags);

extern size_t Rr_GetBufferSize(Rr_Buffer *Buffer);

extern void RR_CC Rr_ReleaseBuffer(Rr_Buffer *Buffer);

extern void *RR_CC Rr_GetMappedBufferData(Rr_Buffer *Buffer);

/* extern void *RR_CC Rr_MapBuffer(Rr_Buffer *Buffer); */

/* extern void RR_CC Rr_UnmapBuffer(Rr_Buffer *Buffer); */

extern void RR_CC
Rr_FlushBufferRange(Rr_Buffer *Buffer, uint64_t Offset, uint64_t Size);

#ifdef __cplusplus
}
#endif

#endif
