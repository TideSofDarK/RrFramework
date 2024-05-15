#include "Rr_Handle.h"

#include "Rr_Memory.h"
#include "Rr_Renderer.h"
#include "Rr_Types.h"

#define SetObjectStorageSize(Type, Index, Ratios, Sizes) Sizes[Index] = sizeof(Type) * Ratios[Index]

void Rr_InitHandleStorage(Rr_Renderer* Renderer, const Rr_ObjectStorageRatios Ratios)
{
    Rr_ObjectStorageRatios Sizes = {0};
    SetObjectStorageSize(Rr_Buffer, RR_HANDLE_TYPE_BUFFER, Ratios, Sizes);
    SetObjectStorageSize(Rr_StaticMesh, RR_HANDLE_TYPE_STATIC_MESH, Ratios, Sizes);
    SetObjectStorageSize(Rr_Image, RR_HANDLE_TYPE_IMAGE, Ratios, Sizes);
    SetObjectStorageSize(Rr_Font, RR_HANDLE_TYPE_FONT, Ratios, Sizes);
    SetObjectStorageSize(Rr_Material, RR_HANDLE_TYPE_MATERIAL, Ratios, Sizes);

    size_t TotalSize = 0;
    for (size_t Index = 0; Index < RR_HANDLE_TYPE_COUNT; ++Index)
    {
        TotalSize += Sizes[Index];
    }

    Renderer->Storage = Rr_Malloc(TotalSize);

    for (size_t Index = 0; Index < RR_HANDLE_TYPE_COUNT; ++Index)
    {
        TotalSize += Sizes[Index];
    }
}

void Rr_CleanupHandleStorage(Rr_Renderer* Renderer)
{

}

void* Rr_CreateHandle(Rr_Renderer* Renderer, Rr_HandleType Type)
{

}

void Rr_DestroyHandle(Rr_Renderer* Renderer, Rr_Handle* Handle, Rr_HandleType Type)
{

}
