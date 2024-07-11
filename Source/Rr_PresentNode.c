#include "Rr_PresentNode.h"

#include "Rr_App.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

Rr_Bool Rr_BatchPresentNode(Rr_App *App, Rr_Graph *Graph, Rr_PresentNode *Node)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_DrawTarget *DrawTarget = Renderer->DrawTarget;

    if (Rr_SyncImage(
            App,
            Graph,
            DrawTarget->Frames[App->Renderer.CurrentFrameIndex]
                .ColorImage->Handle,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT |
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) != RR_TRUE ||
        Rr_SyncImage(
            App,
            Graph,
            DrawTarget->Frames[App->Renderer.CurrentFrameIndex]
                .DepthImage->Handle,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) != RR_TRUE ||
        Rr_SyncImage(
            App,
            Graph,
            Graph->SwapchainImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }

    return RR_TRUE;
}

void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_PresentNode *Node,
    Rr_Arena *Arena)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_DrawTarget *DrawTarget = Renderer->DrawTarget;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    /* Render Dear ImGui if needed. */

    Rr_ImGui *ImGui = &Renderer->ImGui;
    if (ImGui->IsInitialized)
    {
        VkRenderPassBeginInfo RenderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = Renderer->RenderPasses.ColorDepthLoad,
            .framebuffer =
                Renderer->DrawTarget->Frames[Renderer->CurrentFrameIndex]
                    .Framebuffer,
            .renderArea.extent.width =
                Renderer->DrawTarget->Frames[Renderer->CurrentFrameIndex]
                    .ColorImage->Extent.width,
            .renderArea.extent.height =
                Renderer->DrawTarget->Frames[Renderer->CurrentFrameIndex]
                    .ColorImage->Extent.height,
            .clearValueCount = 0,
            .pClearValues = NULL,
        };
        vkCmdBeginRenderPass(
            CommandBuffer,
            &RenderPassBeginInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(
            igGetDrawData(),
            CommandBuffer,
            VK_NULL_HANDLE);

        vkCmdEndRenderPass(CommandBuffer);
    }

    Rr_ImageBarrier ColorImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image =
            DrawTarget->Frames[Renderer->CurrentFrameIndex].ColorImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .AccessMask = VK_ACCESS_TRANSFER_READ_BIT |
                      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT |
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    Rr_ChainImageBarrier(
        &ColorImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    Rr_ImageBarrier SwapchainImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = Graph->SwapchainImage,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_NONE,
        .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    Rr_ChainImageBarrier(
        &SwapchainImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    Rr_BlitColorImage(
        CommandBuffer,
        DrawTarget->Frames[Renderer->CurrentFrameIndex].ColorImage->Handle,
        Graph->SwapchainImage,
        Renderer->SwapchainSize,
        Renderer->SwapchainSize);

    Rr_ChainImageBarrier(
        &SwapchainImageTransition,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
