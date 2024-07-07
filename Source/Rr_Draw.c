#include "Rr_Draw.h"

#include "Rr_App.h"
#include "Rr_Image.h"
#include "Rr_Material.h"
#include "Rr_Mesh.h"

Rr_GraphPass *Rr_CreateGraphPass(
    Rr_App *App,
    Rr_GraphPassInfo *Info,
    char *GlobalsData)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphPass *Pass =
        RR_SLICE_PUSH(&Frame->DrawContextsSlice, &Frame->Arena);
    *Pass = (Rr_GraphPass){
        .Arena = &Frame->Arena,
        .App = App,
        .Info = *Info,
    };

    if (Pass->Info.DrawTarget == NULL)
    {
        Pass->Info.DrawTarget = Renderer->DrawTarget;
        Pass->Info.Viewport.Width = (int32_t)Renderer->SwapchainSize.width;
        Pass->Info.Viewport.Height = (int32_t)Renderer->SwapchainSize.height;
    }

    memcpy(Pass->GlobalsData, GlobalsData, Info->BasePipeline->Sizes.Globals);

    return Pass;
}

void Rr_DrawStaticMesh(
    Rr_GraphPass *Pass,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData)
{
    Rr_Renderer *Renderer = &Pass->App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->PerDrawBuffer.Offset;

    for (size_t PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount;
         ++PrimitiveIndex)
    {
        *RR_SLICE_PUSH(&Pass->DrawPrimitivesSlice, Pass->Arena) =
            (Rr_DrawPrimitiveInfo){
                .PerDrawOffset = Offset,
                .Primitive = StaticMesh->Primitives[PrimitiveIndex],
                .Material = StaticMesh->Materials[PrimitiveIndex],
            };
    }

    Rr_CopyToMappedUniformBuffer(
        Pass->App,
        Frame->PerDrawBuffer.Buffer,
        &Frame->PerDrawBuffer.Offset,
        PerDrawData);
}

void Rr_DrawStaticMeshOverrideMaterials(
    Rr_GraphPass *Pass,
    Rr_Material **OverrideMaterials,
    size_t OverrideMaterialCount,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData)
{
    Rr_Renderer *Renderer = &Pass->App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->PerDrawBuffer.Offset;

    for (size_t PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount;
         ++PrimitiveIndex)
    {
        *RR_SLICE_PUSH(&Pass->DrawPrimitivesSlice, Pass->Arena) =
            (Rr_DrawPrimitiveInfo){
                .PerDrawOffset = Offset,
                .Primitive = StaticMesh->Primitives[PrimitiveIndex],
                .Material = PrimitiveIndex < OverrideMaterialCount
                                ? OverrideMaterials[PrimitiveIndex]
                                : NULL,
            };
    }

    Rr_CopyToMappedUniformBuffer(
        Pass->App,
        Frame->PerDrawBuffer.Buffer,
        &Frame->PerDrawBuffer.Offset,
        PerDrawData);
}

static void Rr_DrawText(Rr_GraphPass *RenderingContext, Rr_DrawTextInfo *Info)
{
    Rr_DrawTextInfo *NewInfo = RR_SLICE_PUSH(
        &RenderingContext->DrawTextsSlice,
        RenderingContext->Arena);
    *NewInfo = *Info;
    if (NewInfo->Font == NULL)
    {
        NewInfo->Font = RenderingContext->App->Renderer.BuiltinFont;
    }
    if (NewInfo->Size == 0.0f)
    {
        NewInfo->Size = NewInfo->Font->DefaultSize;
    }
}

void Rr_DrawCustomText(
    Rr_GraphPass *Pass,
    Rr_Font *Font,
    Rr_String *String,
    Rr_Vec2 Position,
    float Size,
    Rr_DrawTextFlags Flags)
{
    Rr_DrawText(
        Pass,
        &(Rr_DrawTextInfo){ .Font = Font,
                            .String = *String,
                            .Position = Position,
                            .Size = Size,
                            .Flags = Flags });
}

void Rr_DrawDefaultText(Rr_GraphPass *Pass, Rr_String *String, Rr_Vec2 Position)
{
    Rr_DrawText(
        Pass,
        &(Rr_DrawTextInfo){ .String = *String,
                            .Position = Position,
                            .Size = 32.0f,
                            .Flags = 0 });
}

Rr_DrawTarget *Rr_CreateDrawTarget(Rr_App *App, uint32_t Width, uint32_t Height)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_DrawTarget *DrawTarget = Rr_CreateObject(&App->ObjectStorage);

    DrawTarget->ColorImage = Rr_CreateColorAttachmentImage(App, Width, Height);
    DrawTarget->DepthImage = Rr_CreateDepthAttachmentImage(App, Width, Height);

    VkImageView Attachments[2] = { DrawTarget->ColorImage->View,
                                   DrawTarget->DepthImage->View };

    VkFramebufferCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = Renderer->RenderPasses.ColorDepth,
        .height = Height,
        .width = Width,
        .layers = 1,
        .attachmentCount = 2,
        .pAttachments = Attachments,
    };
    vkCreateFramebuffer(
        Renderer->Device,
        &Info,
        NULL,
        &DrawTarget->Framebuffer);

    return DrawTarget;
}

Rr_DrawTarget *Rr_CreateDrawTargetDepthOnly(
    Rr_App *App,
    uint32_t Width,
    uint32_t Height)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_DrawTarget *DrawTarget = Rr_CreateObject(&App->ObjectStorage);

    DrawTarget->DepthImage = Rr_CreateDepthAttachmentImage(App, Width, Height);

    VkImageView Attachments[1] = { DrawTarget->DepthImage->View };

    VkFramebufferCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = Renderer->RenderPasses.Depth,
        .height = Height,
        .width = Width,
        .layers = 1,
        .attachmentCount = SDL_arraysize(Attachments),
        .pAttachments = Attachments,
    };
    vkCreateFramebuffer(
        Renderer->Device,
        &Info,
        NULL,
        &DrawTarget->Framebuffer);

    return DrawTarget;
}

void Rr_DestroyDrawTarget(Rr_App *App, Rr_DrawTarget *DrawTarget)
{
    if (DrawTarget == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroyFramebuffer(Renderer->Device, DrawTarget->Framebuffer, NULL);

    Rr_DestroyImage(App, DrawTarget->ColorImage);
    Rr_DestroyImage(App, DrawTarget->DepthImage);

    Rr_DestroyObject(&App->ObjectStorage, DrawTarget);
}

Rr_DrawTarget *Rr_GetMainDrawTarget(Rr_App *App)
{
    return App->Renderer.DrawTarget;
}
