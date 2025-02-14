#pragma once

#include <Rr/Rr_GLTF.h>

#include "Rr_Memory.h"

#include <Rr/Rr_Asset.h>
#include <Rr/Rr_Platform.h>

struct Rr_UploadContext;
struct Rr_Buffer;

struct Rr_GLTFContext
{
    RR_SLICE(struct Rr_Buffer) Buffers;
    RR_SLICE(struct Rr_Image) Images;
    size_t VertexInputBindingCount;
    Rr_GLTFVertexInputBinding *VertexInputBindings;
    Rr_SpinLock Lock;
    Rr_Arena *Arena;
};

extern Rr_GLTFAsset *Rr_CreateGLTFAsset(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_GLTFContext *GLTFContext,
    Rr_Arena *Arena);
