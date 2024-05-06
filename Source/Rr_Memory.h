#pragma once

#include "Rr_Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef RR_DISABLE_ALLOCA
    #define Rr_StackAlloc(Type, Count) (Type*)alloca(sizeof(Type) * (Count))
    #define Rr_StackFree(Data) \
        do                     \
        {                      \
        }                      \
        while (0)
#else
    #define Rr_StackAlloc(Type, Count) (Type*)Rr_Calloc((Count), sizeof(Type))
    #define Rr_StackFree(Data) \
        do                     \
        {                      \
            Rr_Free(Data)      \
        }                      \
        while (0)
#endif

void* Rr_Malloc(size_t Bytes);
void* Rr_Calloc(size_t Num, size_t Bytes);
void* Rr_Realloc(void* Ptr, size_t Bytes);
void Rr_Free(void* Ptr);

void* Rr_AlignedAlloc(size_t Alignment, size_t Bytes);
void Rr_AlignedFree(void* Ptr);

#ifdef __cplusplus
}
#endif
