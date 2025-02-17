#pragma once

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Image.h>

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
    RR_BUFFER_FLAGS_STAGING_BIT = (1 << 7),
    RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT = (1 << 8),
    RR_BUFFER_FLAGS_MAPPED_BIT = (1 << 9),
    RR_BUFFER_FLAGS_PER_FRAME_BIT = (1 << 10),
} Rr_BufferFlagsBits;
typedef uint32_t Rr_BufferFlags;

extern Rr_Buffer *Rr_CreateBuffer(
    Rr_App *App,
    size_t Size,
    Rr_BufferFlags Flags);

extern void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer);

extern void *Rr_GetMappedBufferData(Rr_App *App, Rr_Buffer *Buffer);

extern void *Rr_MapBuffer(Rr_App *App, Rr_Buffer *Buffer);

extern void Rr_UnmapBuffer(Rr_App *App, Rr_Buffer *Buffer);

extern void Rr_FlushBufferRange(
    Rr_App *App,
    Rr_Buffer *Buffer,
    size_t Offset,
    size_t Size);

extern void Rr_UploadToDeviceBufferImmediate(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    Rr_Data Data);

#ifdef __cplusplus
}
#endif
