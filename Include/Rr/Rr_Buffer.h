#pragma once

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Image.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Buffer Rr_Buffer;

extern Rr_Buffer *Rr_CreateDeviceVertexBuffer(Rr_App *App, size_t Size);

extern Rr_Buffer *Rr_CreateDeviceIndexBuffer(Rr_App *App, size_t Size);

extern void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer);

extern void Rr_UploadToDeviceBufferImmediate(Rr_App *App, Rr_Buffer *DstBuffer, Rr_Data Data);

#ifdef __cplusplus
}
#endif
