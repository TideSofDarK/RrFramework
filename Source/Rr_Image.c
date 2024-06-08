#include "Rr_Image.h"

#include "Rr_App_Internal.h"
#include "Rr_Image_Internal.h"

void Rr_DestroyImage(Rr_App* App, Rr_Image* AllocatedImage)
{
    if (AllocatedImage == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroyImageView(Renderer->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Renderer->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);

    Rr_DestroyObject(&Renderer->ObjectStorage, AllocatedImage);
}
