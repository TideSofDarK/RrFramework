#pragma once

#include "Rr_Defines.h"

struct Rr_Renderer;

typedef enum Rr_HandleType {
    RR_HANDLE_TYPE_BUFFER,
    RR_HANDLE_TYPE_STATIC_MESH,
    RR_HANDLE_TYPE_IMAGE,
    RR_HANDLE_TYPE_FONT,
    RR_HANDLE_TYPE_MATERIAL,
    RR_HANDLE_TYPE_COUNT,
} Rr_HandleType;

typedef struct Rr_Handle Rr_Handle;

typedef size_t Rr_ObjectStorageRatios[RR_HANDLE_TYPE_COUNT];

void Rr_InitHandleStorage(struct Rr_Renderer* Renderer, const Rr_ObjectStorageRatios Ratios);
void Rr_CleanupHandleStorage(struct Rr_Renderer* Renderer);

void* Rr_CreateHandle(struct Rr_Renderer* Renderer, Rr_HandleType Type);
void Rr_DestroyHandle(struct Rr_Renderer* Renderer, Rr_Handle* Handle, Rr_HandleType Type);
