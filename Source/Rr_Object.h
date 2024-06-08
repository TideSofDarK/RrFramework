#pragma once

#include "Rr/Rr_Defines.h"

struct Rr_Renderer;

typedef union Rr_Object Rr_Object;

typedef struct Rr_ObjectStorage Rr_ObjectStorage;
struct Rr_ObjectStorage
{
    void* Storage;
    void* NextObject;
    size_t ObjectCount;
};

extern Rr_ObjectStorage Rr_CreateObjectStorage();
extern void Rr_DestroyObjectStorage(Rr_ObjectStorage* Storage);

extern void* Rr_CreateObject(Rr_ObjectStorage* Storage);
extern void Rr_DestroyObject(Rr_ObjectStorage* Storage, void* Object);
extern usize Rr_CalculateObjectStorageSize(usize Count);
