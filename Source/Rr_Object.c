#include "Rr_Object.h"

#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Mesh.h"
#include "Rr_Draw.h"
#include "Rr_Text.h"
#include "Rr_Pipeline.h"
#include "Rr_Material.h"

/* @TODO: Make bValid field! */
union Rr_Object
{
    Rr_Buffer Buffer;
    Rr_Primitive Primitive;
    Rr_StaticMesh StaticMesh;
    Rr_SkeletalMesh SkeletalMesh;
    Rr_Image Image;
    Rr_Font Font;
    Rr_Material Material;
    Rr_GenericPipeline GenericPipeline;
    Rr_DrawTarget DrawTarget;
    void* Next;
};

Rr_ObjectStorage Rr_CreateObjectStorage()
{
    Rr_ObjectStorage ObjectStorage = { 0 };
    ObjectStorage.Storage = Rr_Calloc(1, Rr_CalculateObjectStorageSize(RR_MAX_OBJECTS));
    ObjectStorage.NextObject = ObjectStorage.Storage;

    return ObjectStorage;
}

void Rr_DestroyObjectStorage(Rr_ObjectStorage* Storage)
{
    SDL_LockSpinlock(&Storage->Lock);

    Rr_Free(Storage->Storage);
    Storage->Storage = NULL;

    SDL_UnlockSpinlock(&Storage->Lock);
}

void* Rr_CreateObject(Rr_ObjectStorage* Storage)
{
    SDL_LockSpinlock(&Storage->Lock);

    Rr_Object* NewObject = (Rr_Object*)Storage->NextObject;
    if (NewObject->Next == NULL)
    {
        Storage->NextObject = NewObject + 1;
    }
    else
    {
        Storage->NextObject = NewObject->Next;
    }
    Storage->ObjectCount++;

    SDL_UnlockSpinlock(&Storage->Lock);

    SDL_zerop(NewObject);
    return NewObject;
}

void Rr_DestroyObject(Rr_ObjectStorage* Storage, void* Object)
{
    SDL_LockSpinlock(&Storage->Lock);

    Rr_Object* DestroyedObject = (Rr_Object*)Object;
    DestroyedObject->Next = Storage->NextObject;
    Storage->NextObject = DestroyedObject;
    Storage->ObjectCount--;

    SDL_UnlockSpinlock(&Storage->Lock);
}

usize Rr_CalculateObjectStorageSize(usize Count)
{
    return sizeof(Rr_Object) * Count;
}
