#include "Rr_Draw.h"

#include "Rr_Vulkan.h"
#include "Rr_Types.h"
#include "Rr_Renderer.h"
#include "Rr_Memory.h"
#include "Rr_Object.h"
#include "Rr_ImageTools.h"

Rr_RenderingContext* Rr_CreateDrawContext(Rr_App* App, Rr_DrawContextInfo* Info, const byte* GlobalsData)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    Rr_RenderingContext* RenderingContext = Rr_SlicePush(&Frame->RenderingContextsSlice, &Frame->Arena);
    *RenderingContext = (Rr_RenderingContext){
        .Arena = &Frame->Arena,
        .Renderer = Renderer,
        .Info = *Info,
    };
    SDL_memcpy(RenderingContext->GlobalsData, GlobalsData, Info->Sizes.Globals);

    return RenderingContext;
}

void Rr_DrawStaticMesh(
    Rr_RenderingContext* RenderingContext,
    Rr_StaticMesh* StaticMesh,
    Rr_Data DrawData)
{
    Rr_Renderer* Renderer = RenderingContext->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->DrawBuffer.Offset;

    for (usize PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount; ++PrimitiveIndex)
    {
        *Rr_SlicePush(&RenderingContext->PrimitivesSlice, RenderingContext->Arena) = (Rr_DrawPrimitiveInfo){
            .OffsetIntoDrawBuffer = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = StaticMesh->Materials[PrimitiveIndex]
        };
    }

    Rr_CopyToMappedUniformBuffer(RenderingContext->Renderer, Frame->DrawBuffer.Buffer, DrawData.Data, DrawData.Size, &Frame->DrawBuffer.Offset);
}

void Rr_DrawStaticMeshOverrideMaterials(
    Rr_RenderingContext* RenderingContext,
    Rr_Material** OverrideMaterials,
    usize OverrideMaterialCount,
    Rr_StaticMesh* StaticMesh,
    Rr_Data DrawData)
{
    Rr_Renderer* Renderer = RenderingContext->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->DrawBuffer.Offset;

    for (usize PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount; ++PrimitiveIndex)
    {
        *Rr_SlicePush(&RenderingContext->PrimitivesSlice, RenderingContext->Arena) = (Rr_DrawPrimitiveInfo){
            .OffsetIntoDrawBuffer = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = PrimitiveIndex < OverrideMaterialCount ? OverrideMaterials[PrimitiveIndex] : NULL
        };
    }

    Rr_CopyToMappedUniformBuffer(RenderingContext->Renderer, Frame->DrawBuffer.Buffer, DrawData.Data, DrawData.Size, &Frame->DrawBuffer.Offset);
}

static void Rr_DrawText(Rr_RenderingContext* RenderingContext, const Rr_DrawTextInfo* Info)
{
    Rr_DrawTextInfo* NewInfo = Rr_SlicePush(&RenderingContext->DrawTextSlice, RenderingContext->Arena);
    *NewInfo = *Info;
    if (NewInfo->Font == NULL)
    {
        NewInfo->Font = RenderingContext->Renderer->BuiltinFont;
    }
    if (NewInfo->Size == 0.0f)
    {
        NewInfo->Size = NewInfo->Font->DefaultSize;
    }
}

void Rr_DrawCustomText(
    Rr_RenderingContext* RenderingContext,
    Rr_Font* Font,
    Rr_String* String,
    Rr_Vec2 Position,
    f32 Size,
    Rr_DrawTextFlags Flags)
{
    Rr_DrawText(
        RenderingContext,
        &(Rr_DrawTextInfo){
            .Font = Font,
            .String = *String,
            .Position = Position,
            .Size = Size,
            .Flags = Flags });
}

void Rr_DrawDefaultText(Rr_RenderingContext* RenderingContext, Rr_String* String, Rr_Vec2 Position)
{
    Rr_DrawText(
        RenderingContext,
        &(Rr_DrawTextInfo){
            .String = *String,
            .Position = Position,
            .Size = 32.0f,
            .Flags = 0 });
}

Rr_DrawTarget* Rr_CreateDrawTarget(Rr_App* App, u32 Width, u32 Height)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DrawTarget* DrawTarget = Rr_CreateObject(Renderer);

    DrawTarget->ColorImage = Rr_CreateColorAttachmentImage(App, Width, Height);
    DrawTarget->DepthImage = Rr_CreateDepthAttachmentImage(App, Width, Height);

    VkImageView Attachments[2] = { DrawTarget->ColorImage->View, DrawTarget->DepthImage->View };

    const VkFramebufferCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = Renderer->RenderPass,
        .height = Height,
        .width = Width,
        .layers = 1,
        .attachmentCount = 2,
        .pAttachments = Attachments
    };
    vkCreateFramebuffer(Renderer->Device, &Info, NULL, &DrawTarget->Framebuffer);

    return DrawTarget;
}

void Rr_DestroyDrawTarget(Rr_App* App, Rr_DrawTarget* DrawTarget)
{
    Rr_Renderer* Renderer = &App->Renderer;
    vkDestroyFramebuffer(Renderer->Device, DrawTarget->Framebuffer, NULL);
    Rr_DestroyImage(App, DrawTarget->ColorImage);
    Rr_DestroyImage(App, DrawTarget->DepthImage);

    Rr_DestroyObject(Renderer, DrawTarget);
}

Rr_DrawTarget* Rr_GetMainDrawTarget(Rr_App* App)
{
    return App->Renderer.DrawTarget;
}
