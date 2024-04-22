#include "Rr_Material.h"

Rr_Material Rr_CreateMaterial(Rr_Renderer* Renderer, Rr_Image** Textures, size_t TextureCount)
{
    Rr_Material Material = {
        .TextureCount = TextureCount,
        .Buffer = Rr_CreateBuffer(Renderer->Allocator, 128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO, false)
    };

    TextureCount = SDL_min(TextureCount, RR_MAX_TEXTURES_PER_MATERIAL);
    for (int Index = 0; Index < TextureCount; ++Index)
    {
        Material.Textures[Index] = Textures[Index];
    }

    return Material;
}

void Rr_DestroyMaterial(Rr_Renderer* Renderer, Rr_Material* Material)
{
    Rr_DestroyBuffer(&Material->Buffer, Renderer->Allocator);
}
