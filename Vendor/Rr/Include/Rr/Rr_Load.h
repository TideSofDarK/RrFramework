#pragma once

#include "Rr_Defines.h"

typedef struct Rr_LoadingContext Rr_LoadingContext;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_Image Rr_Image;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;
typedef struct Rr_Renderer Rr_Renderer;
typedef void (*Rr_LoadingCallback)(Rr_Renderer* Renderer, u32 Status);

Rr_LoadingContext* Rr_CreateLoadingContext(Rr_Renderer* Renderer, size_t InitialTaskCount);
void Rr_LoadRGBA8FromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_Image* OutImage);
void Rr_LoadMeshFromOBJ(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_MeshBuffers* OutMeshBuffers);
void Rr_Load(Rr_LoadingContext* LoadingContext, Rr_LoadingCallback LoadingCallback);
f32 Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext);
