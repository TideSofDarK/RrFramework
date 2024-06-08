#include "Rr_Mesh_Internal.h"

#include "Rr_Object.h"
#include "Rr_Material.h"
#include "Rr_App_Internal.h"

void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* StaticMesh)
{
    if (StaticMesh == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    for (usize Index = 0; Index < StaticMesh->PrimitiveCount; ++Index)
    {
        Rr_DestroyPrimitive(App, StaticMesh->Primitives[Index]);
    }

    for (usize Index = 0; Index < StaticMesh->MaterialCount; ++Index)
    {
        Rr_DestroyMaterial(App, StaticMesh->Materials[Index]);
    }

    Rr_DestroyObject(&Renderer->ObjectStorage, StaticMesh);
}
