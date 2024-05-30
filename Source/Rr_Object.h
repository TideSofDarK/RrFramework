#pragma once

#include "Rr_Defines.h"

struct Rr_Renderer;

typedef union Rr_Object Rr_Object;

extern void* Rr_CreateObject(struct Rr_Renderer* Renderer);
extern void Rr_DestroyObject(struct Rr_Renderer* Renderer, void* Object);
extern usize Rr_CalculateObjectStorageSize(usize Count);
