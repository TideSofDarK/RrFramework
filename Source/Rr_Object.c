#include "Rr_Object.h"

#include "Rr_Types.h"
#include "Rr_Renderer.h"

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

void* Rr_CreateObject(Rr_Renderer* Renderer)
{
    Rr_Object* NewObject = (Rr_Object*)Renderer->NextObject;
    if (NewObject->Next == NULL)
    {
        Renderer->NextObject = NewObject + 1;
    }
    else
    {
        Renderer->NextObject = NewObject->Next;
    }
    Renderer->ObjectCount++;
    return NewObject;
}

void Rr_DestroyObject(Rr_Renderer* Renderer, void* Object)
{
    Rr_Object* DestroyedObject = (Rr_Object*)Object;
    DestroyedObject->Next = Renderer->NextObject;
    Renderer->NextObject = DestroyedObject;
    Renderer->ObjectCount--;
}

usize Rr_CalculateObjectStorageSize(usize Count)
{
    return sizeof(Rr_Object) * Count;
}
