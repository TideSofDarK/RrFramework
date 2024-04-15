#pragma once

#include "RrDefines.h"

void* Rr_Malloc(size_t Bytes);
void* Rr_Calloc(size_t Num, size_t Bytes);
void* Rr_Realloc(void* Ptr, size_t Bytes);
void Rr_Free(void* Ptr);
