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
    RR_BUFFER_USAGE_UNIFORM_BUFFER_BIT = (1 << 0),
    RR_BUFFER_USAGE_STORAGE_BUFFER_BIT = (1 << 1),
    RR_BUFFER_USAGE_VERTEX_BUFFER_BIT = (1 << 2),
    RR_BUFFER_USAGE_INDEX_BUFFER_BIT = (1 << 3),
    RR_BUFFER_USAGE_INDIRECT_BUFFER_BIT = (1 << 4),
} Rr_BufferUsageBits;
typedef int Rr_BufferUsage;

extern Rr_Buffer *Rr_CreateBufferNEW(
    Rr_App *App,
    size_t Size,
    Rr_BufferUsage Usage,
    bool Buffered);

extern void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer);

extern void Rr_UploadToDeviceBufferImmediate(Rr_App *App, Rr_Buffer *DstBuffer, Rr_Data Data);

#ifdef __cplusplus
}
#endif
