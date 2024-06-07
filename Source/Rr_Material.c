#include "Rr_Material.h"

#include "Rr_Renderer.h"
#include "Rr_Buffer.h"
#include "Rr_Types.h"
#include "Rr_Object.h"

#include <SDL3/SDL_stdinc.h>

Rr_Material* Rr_CreateMaterial(Rr_App* App, Rr_GenericPipeline* GenericPipeline, Rr_Image** Textures, usize TextureCount)
{
    Rr_Material* Material = Rr_CreateObject(&App->Renderer.ObjectStorage);
    *Material = (Rr_Material){
        .GenericPipeline = GenericPipeline,
        .TextureCount = TextureCount,
        .Buffer = Rr_CreateBuffer(
            &App->Renderer,
            GenericPipeline->Sizes.Material,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            false)
    };

    TextureCount = SDL_min(TextureCount, RR_MAX_TEXTURES_PER_MATERIAL);
    for (usize Index = 0; Index < TextureCount; ++Index)
    {
        Material->Textures[Index] = Textures[Index];
    }

    return Material;
}

void Rr_DestroyMaterial(Rr_App* App, Rr_Material* Material)
{
    if (Material == NULL)
    {
        return;
    }

    if (Material->bOwning)
    {
        for (usize TextureIndex = 0; TextureIndex < Material->TextureCount; ++TextureIndex)
        {
            Rr_DestroyImage(App, Material->Textures[TextureIndex]);
        }
    }

    Rr_DestroyBuffer(&App->Renderer, Material->Buffer);

    Rr_DestroyObject(&App->Renderer.ObjectStorage, Material);
}
