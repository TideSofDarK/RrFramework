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

#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Buffer Rr_Buffer;
typedef struct Rr_StagingBuffer Rr_StagingBuffer;

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

extern Rr_Buffer *Rr_CreateBuffer(size_t Size, Rr_BufferFlags Flags);

extern void Rr_DestroyBuffer(Rr_Buffer *Buffer);

extern void *Rr_GetMappedBufferData(Rr_Buffer *Buffer);

extern void *Rr_MapBuffer(Rr_Buffer *Buffer);

extern void Rr_UnmapBuffer(Rr_Buffer *Buffer);

extern void Rr_FlushBufferRange(Rr_Buffer *Buffer, size_t Offset, size_t Size);

extern void Rr_UploadToDeviceBufferImmediate(
    Rr_Buffer *DstBuffer,
    Rr_Data Data);

#ifdef __cplusplus
}
#endif
