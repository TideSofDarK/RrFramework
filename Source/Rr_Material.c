#include "Rr_Material.h"

#include "Rr_App.h"
#include "Rr_Pipeline.h"

#include <SDL3/SDL_stdinc.h>

Rr_Material *Rr_CreateMaterial(
    Rr_App *App,
    Rr_GenericPipeline *GenericPipeline,
    Rr_Image **Textures,
    size_t TextureCount)
{
    Rr_Material *Material = Rr_CreateObject(&App->ObjectStorage);
    *Material = (Rr_Material){
        .GenericPipeline = GenericPipeline,
        .TextureCount = TextureCount,
        .Buffer = Rr_CreateBuffer(
            App,
            GenericPipeline->Sizes.Material,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            RR_FALSE),
    };

    TextureCount = SDL_min(TextureCount, RR_MAX_TEXTURES_PER_MATERIAL);
    for (size_t Index = 0; Index < TextureCount; ++Index)
    {
        Material->Textures[Index] = Textures[Index];
    }

    return Material;
}

void Rr_DestroyMaterial(Rr_App *App, Rr_Material *Material)
{
    if (Material == NULL)
    {
        return;
    }

    if (Material->bOwning)
    {
        for (size_t TextureIndex = 0; TextureIndex < Material->TextureCount;
             ++TextureIndex)
        {
            Rr_DestroyImage(App, Material->Textures[TextureIndex]);
        }
    }

    Rr_DestroyBuffer(App, Material->Buffer);

    Rr_DestroyObject(&App->ObjectStorage, Material);
}
