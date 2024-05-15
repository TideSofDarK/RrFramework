#include "Rr_Material.h"

#include "Rr_App.h"
#include "Rr_Renderer.h"
#include "Rr_Buffer.h"
#include "Rr_Memory.h"
#include "Rr_Types.h"

#include <SDL3/SDL_stdinc.h>

Rr_Material* Rr_CreateMaterial(Rr_App* App, Rr_Image** Textures, size_t TextureCount)
{
    Rr_Material* Material = Rr_CreateObject(&App->Renderer);
    *Material = (Rr_Material){
        .TextureCount = TextureCount,
        .Buffer = Rr_CreateBuffer(&App->Renderer, 128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false)
    };

    TextureCount = SDL_min(TextureCount, RR_MAX_TEXTURES_PER_MATERIAL);
    for (size_t Index = 0; Index < TextureCount; ++Index)
    {
        Material->Textures[Index] = Textures[Index];
    }

    return Material;
}

void Rr_DestroyMaterial(Rr_App* App, Rr_Material* Material)
{
    Rr_DestroyBuffer(&App->Renderer, Material->Buffer);

    Rr_DestroyObject(&App->Renderer, Material);
}
