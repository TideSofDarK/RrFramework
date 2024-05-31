#include "Rr_Draw.h"

#include "Rr_Vulkan.h"
#include "Rr_Types.h"
#include "Rr_Renderer.h"
#include "Rr_Memory.h"
#include "Rr_Object.h"
#include "Rr_ImageTools.h"

Rr_DrawContext* Rr_CreateDrawContext(Rr_App* App, Rr_DrawContextInfo* Info, const byte* GlobalsData)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DrawContext* RenderingContext = Rr_SlicePush(&Frame->DrawContextsSlice, &Frame->Arena);
    *RenderingContext = (Rr_DrawContext){
        .Arena = &Frame->Arena,
        .Renderer = Renderer,
        .Info = *Info,
    };
    SDL_memcpy(RenderingContext->GlobalsData, GlobalsData, Info->Sizes.Globals);

    return RenderingContext;
}

void Rr_DrawStaticMesh(
    Rr_DrawContext* DrawContext,
    Rr_StaticMesh* StaticMesh,
    Rr_Data DrawData)
{
    Rr_Renderer* Renderer = DrawContext->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->DrawBuffer.Offset;

    for (usize PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount; ++PrimitiveIndex)
    {
        *Rr_SlicePush(&DrawContext->DrawPrimitivesSlice, DrawContext->Arena) = (Rr_DrawPrimitiveInfo){
            .OffsetIntoDrawBuffer = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = StaticMesh->Materials[PrimitiveIndex]
        };
    }

    Rr_CopyToMappedUniformBuffer(DrawContext->Renderer, Frame->DrawBuffer.Buffer, DrawData.Data, DrawData.Size, &Frame->DrawBuffer.Offset);
}

void Rr_DrawStaticMeshOverrideMaterials(
    Rr_DrawContext* DrawContext,
    Rr_Material** OverrideMaterials,
    usize OverrideMaterialCount,
    Rr_StaticMesh* StaticMesh,
    Rr_Data DrawData)
{
    Rr_Renderer* Renderer = DrawContext->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->DrawBuffer.Offset;

    for (usize PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount; ++PrimitiveIndex)
    {
        *Rr_SlicePush(&DrawContext->DrawPrimitivesSlice, DrawContext->Arena) = (Rr_DrawPrimitiveInfo){
            .OffsetIntoDrawBuffer = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = PrimitiveIndex < OverrideMaterialCount ? OverrideMaterials[PrimitiveIndex] : NULL
        };
    }

    Rr_CopyToMappedUniformBuffer(DrawContext->Renderer, Frame->DrawBuffer.Buffer, DrawData.Data, DrawData.Size, &Frame->DrawBuffer.Offset);
}

static void Rr_DrawText(Rr_DrawContext* RenderingContext, const Rr_DrawTextInfo* Info)
{
    Rr_DrawTextInfo* NewInfo = Rr_SlicePush(&RenderingContext->DrawTextsSlice, RenderingContext->Arena);
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
    Rr_DrawContext* DrawContext,
    Rr_Font* Font,
    Rr_String* String,
    Rr_Vec2 Position,
    f32 Size,
    Rr_DrawTextFlags Flags)
{
    Rr_DrawText(
        DrawContext,
        &(Rr_DrawTextInfo){
            .Font = Font,
            .String = *String,
            .Position = Position,
            .Size = Size,
            .Flags = Flags });
}

void Rr_DrawDefaultText(Rr_DrawContext* DrawContext, Rr_String* String, Rr_Vec2 Position)
{
    Rr_DrawText(
        DrawContext,
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
