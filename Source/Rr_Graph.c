#include "Rr_Graph.h"

#include "Rr_App.h"
#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <SDL3/SDL.h>

#include <assert.h>

static VkAccessFlags AllVulkanWrites =
    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT |
    VK_ACCESS_MEMORY_WRITE_BIT;

static Rr_AllocatedBuffer *Rr_GetGraphBuffer(
    Rr_Graph *Graph,
    Rr_GraphBuffer Handle)
{
    return Graph->Resources.Data[Handle.Values.Index].Allocated;
}

static Rr_AllocatedImage *Rr_GetGraphImage(
    Rr_Graph *Graph,
    Rr_GraphImage Handle)
{
    return Graph->Resources.Data[Handle.Values.Index].Allocated;
}

static void Rr_ExecutePresentNode(
    Rr_Renderer *Renderer,
    Rr_PresentNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &Renderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_AllocatedImage *Image =
        Rr_GetGraphImage(((Rr_GraphNode *)Node)->Graph, Node->ImageHandle);
    Rr_Image *Container = Image->Container;

    VkDescriptorSet DescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Device,
        Renderer->PresentLayout->SetLayouts[0]->Handle);

    VkSampler Sampler = Node->Sampler->Handle;
    Rr_DescriptorWriter *Writer =
        Rr_CreateDescriptorWriter(0, 1, 0, Scratch.Arena);
    Rr_WriteCombinedImageSamplerDescriptor(
        Writer,
        0,
        0,
        Image->View,
        Sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Rr_UpdateDescriptorSet(Writer, Device, DescriptorSet);

    VkImage SwapchainImage =
        Rr_GetCurrentAllocatedImage(Renderer, &Frame->VirtualSwapchainImage)
            ->Handle;

    Device->CmdPipelineBarrier(
        CommandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = SwapchainImage,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });
    VkClearValue ClearValue = { 0 };
    memcpy(
        &ClearValue.color.float32,
        Node->ColorClear.Elements,
        sizeof(Rr_Vec4));
    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Frame->SwapchainFramebuffer,
        .renderArea = (VkRect2D){ { 0, 0 },
                                  { Renderer->Swapchain.Extent.width,
                                    Renderer->Swapchain.Extent.height } },
        .renderPass = Renderer->PresentRenderPass,
        .clearValueCount = 1,
        .pClearValues = &ClearValue,
    };
    Device->CmdBeginRenderPass(
        CommandBuffer,
        &RenderPassBeginInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    float X = 0;
    float Y = 0;
    float Width = 0;
    float Height = 0;

    float SwapchainWidth = Renderer->Swapchain.Extent.width;
    float SwapchainHeight = Renderer->Swapchain.Extent.height;
    float ImageWidth = Container->Extent.width;
    float ImageHeight = Container->Extent.height;
    float SwapchainRatio = SwapchainWidth / SwapchainHeight;
    float ImageRatio = ImageWidth / ImageHeight;

    switch(Node->Mode)
    {
        case RR_PRESENT_MODE_FIT:
        {
            if(SwapchainRatio > ImageRatio)
            {
                Width = (ImageWidth / ImageHeight) * SwapchainHeight;
                Height = SwapchainHeight;
            }
            else
            {
                Width = SwapchainWidth;
                Height = (ImageHeight / ImageWidth) * SwapchainWidth;
            }
            X = (SwapchainWidth / 2.0f) - (Width / 2.0f);
            Y = (SwapchainHeight / 2.0f) - (Height / 2.0f);
        }
        break;
        default:
        {
            Width = Renderer->Swapchain.Extent.width;
            Height = Renderer->Swapchain.Extent.height;
        }
        break;
    }

    Device->CmdSetViewport(
        CommandBuffer,
        0,
        1,
        &(VkViewport){
            .x = X,
            .y = Y,
            .width = Width,
            .height = Height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

    Device->CmdSetScissor(
        CommandBuffer,
        0,
        1,
        &(VkRect2D){
            .offset.x = X,
            .offset.y = Y,
            .extent.width = Width,
            .extent.height = Height,
        });

    Device->CmdBindPipeline(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        Renderer->PresentPipeline->Handle);

    Device->CmdBindDescriptorSets(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        Renderer->PresentPipeline->Layout->Handle,
        0,
        1,
        &DescriptorSet,
        0,
        NULL);

    Device->CmdDraw(CommandBuffer, 3, 1, 0, 0);

    Device->CmdEndRenderPass(CommandBuffer);

    Device->CmdPipelineBarrier(
        CommandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = SwapchainImage,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = 0,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });

    Rr_DestroyScratch(Scratch);
}

static void Rr_ExecuteTransferNode(
    Rr_Renderer *Renderer,
    Rr_Graph *Graph,
    Rr_TransferNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &Renderer->Device;

    for(size_t Index = 0; Index < Node->Transfers.Count; ++Index)
    {
        Rr_Transfer *Transfer = Node->Transfers.Data + Index;

        VkBuffer SrcBuffer =
            Rr_GetGraphBuffer(Graph, Transfer->SrcBuffer)->Handle;
        VkBuffer DstBuffer =
            Rr_GetGraphBuffer(Graph, Transfer->DstBuffer)->Handle;

        VkBufferCopy Copy = { .size = Transfer->Size,
                              .srcOffset = Transfer->SrcOffset,
                              .dstOffset = Transfer->DstOffset };
        Device->CmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &Copy);
    }
}

static inline bool Rr_ClampBlitRect(Rr_IntVec4 *Rect, VkExtent3D *Extent)
{
    Rect->X = RR_CLAMP(0, Rect->X, (int)Extent->width);
    Rect->Y = RR_CLAMP(0, Rect->Y, (int)Extent->height);
    Rect->Width = RR_CLAMP(0, Rect->Width, (int)Extent->width - Rect->X);
    Rect->Height = RR_CLAMP(0, Rect->Height, (int)Extent->height - Rect->Y);

    return Rect->Width > 0 && Rect->Height > 0;
}

static void Rr_ExecuteBlitNode(
    Rr_Renderer *Renderer,
    Rr_Graph *Graph,
    Rr_BlitNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &Renderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_AllocatedImage *SrcImage =
        Rr_GetGraphImage(&Frame->Graph, Node->SrcImageHandle);
    Rr_AllocatedImage *DstImage =
        Rr_GetGraphImage(&Frame->Graph, Node->DstImageHandle);

    if(Rr_ClampBlitRect(&Node->SrcRect, &SrcImage->Container->Extent) &&
       Rr_ClampBlitRect(&Node->DstRect, &DstImage->Container->Extent))
    {
        Rr_BlitColorImage(
            Device,
            CommandBuffer,
            SrcImage->Handle,
            DstImage->Handle,
            Node->SrcRect,
            Node->DstRect,
            Node->AspectMask);
    }
}

static void Rr_ExecuteComputeNode(
    Rr_Renderer *Renderer,
    Rr_Graph *Graph,
    Rr_ComputeNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &Renderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_ComputePipeline *Pipeline = NULL;
    Rr_DescriptorsState DescriptorsState = { 0 };

    for(Rr_NodeFunction *Function = Node->Encoded.EncodedFirst;
        Function != NULL;
        Function = Function->Next)
    {
        switch(Function->Type)
        {
            case RR_NODE_FUNCTION_TYPE_BIND_COMPUTE_PIPELINE:
            {
                Pipeline = *(Rr_ComputePipeline **)Function->Args;
                Device->CmdBindPipeline(
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    Pipeline->Handle);
                Rr_InvalidateDescriptorState(
                    &DescriptorsState,
                    Pipeline->Layout);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DISPATCH:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    &Frame->DescriptorAllocator,
                    Pipeline->Layout,
                    Device,
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_COMPUTE);
                Rr_DispatchArgs *Args = Function->Args;
                Device->CmdDispatch(
                    CommandBuffer,
                    Args->GroupCountX,
                    Args->GroupCountY,
                    Args->GroupCountZ);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_SAMPLER:
            {
                Rr_BindSamplerArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_SAMPLER,
                        .Sampler = Args->Sampler->Handle,
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE:
            {
                Rr_BindSampledImageArgs *Args = Function->Args;
                VkImageView ImageView =
                    Rr_GetGraphImage(Graph, Args->ImageHandle)->View;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE,
                        .Image =
                            {
                                .View = ImageView,
                                .Layout = Args->Layout,
                            },
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER:
            {
                Rr_BindCombinedImageSamplerArgs *Args = Function->Args;
                VkImageView ImageView =
                    Rr_GetGraphImage(Graph, Args->ImageHandle)->View;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                        .Image =
                            {
                                .View = ImageView,
                                .Sampler = Args->Sampler->Handle,
                                .Layout = Args->Layout,
                            },
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER:
            {
                Rr_BindUniformBufferArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
                        .Buffer =
                            {
                                .Handle = Rr_GetGraphBuffer(
                                              Graph,
                                              Args->BufferHandle)
                                              ->Handle,
                                .Size = Args->Size,
                                .Offset = Args->Offset,
                            },
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER:
            {
                Rr_BindStorageBufferArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER,
                        .Buffer =
                            {
                                .Handle = Rr_GetGraphBuffer(
                                              Graph,
                                              Args->BufferHandle)
                                              ->Handle,
                                .Size = Args->Size,
                                .Offset = Args->Offset,
                            },
                    });
            }
            break;
            default:
            {
            }
            break;
        }
    }

    Rr_DestroyScratch(Scratch);
}

static void Rr_ExecuteGraphicsNode(
    Rr_Renderer *Renderer,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &Renderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_IntVec4 Viewport = { 0 };
    Viewport.Width = INT32_MAX;
    Viewport.Height = INT32_MAX;

    /* Line up appropriate clear values. */

    uint32_t AttachmentCount =
        Node->ColorTargetCount + (Node->DepthTarget ? 1 : 0);

    Rr_Attachment *Attachments =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, Rr_Attachment, AttachmentCount);
    VkImageView *ImageViews =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImageView, AttachmentCount);
    VkClearValue *ClearValues =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkClearValue, AttachmentCount);
    for(uint32_t Index = 0; Index < Node->ColorTargetCount; ++Index)
    {
        Rr_ColorTarget *ColorTarget = &Node->ColorTargets[Index];
        VkClearValue *ClearValue = &ClearValues[ColorTarget->Slot];
        memcpy(ClearValue, &ColorTarget->Clear, sizeof(VkClearValue));
        Attachments[ColorTarget->Slot] = (Rr_Attachment){
            .LoadOp = ColorTarget->LoadOp,
            .StoreOp = ColorTarget->StoreOp,
        };
        Rr_AllocatedImage *ColorImage =
            Rr_GetGraphImage(Graph, Node->ColorImages[Index]);
        ImageViews[ColorTarget->Slot] = ColorImage->View;

        Viewport.Width = RR_MIN(
            Viewport.Width,
            (int32_t)ColorImage->Container->Extent.width);
        Viewport.Height = RR_MIN(
            Viewport.Height,
            (int32_t)ColorImage->Container->Extent.height);
    }
    if(Node->DepthTarget != NULL)
    {
        size_t DepthIndex = AttachmentCount - 1;
        Rr_DepthTarget *DepthTarget = Node->DepthTarget;
        VkClearValue *ClearValue = &ClearValues[DepthIndex];
        memcpy(ClearValue, &DepthTarget->Clear, sizeof(VkClearValue));
        Attachments[DepthIndex] = (Rr_Attachment){
            .LoadOp = DepthTarget->LoadOp,
            .StoreOp = DepthTarget->StoreOp,
            .Depth = true,
        };
        Rr_AllocatedImage *DepthImage =
            Rr_GetGraphImage(Graph, Node->DepthImage);
        ImageViews[DepthIndex] = DepthImage->View;

        Viewport.Width = RR_MIN(
            Viewport.Width,
            (int32_t)DepthImage->Container->Extent.width);
        Viewport.Height = RR_MIN(
            Viewport.Height,
            (int32_t)DepthImage->Container->Extent.height);
    }

    /* Begin render pass. */

    Rr_RenderPassInfo RenderPassInfo = {
        .AttachmentCount = AttachmentCount,
        .Attachments = Attachments,
    };
    VkRenderPass RenderPass = Rr_GetRenderPass(Renderer, &RenderPassInfo);
    VkFramebuffer Framebuffer = Rr_GetFramebufferViews(
        Renderer,
        RenderPass,
        ImageViews,
        AttachmentCount,
        (VkExtent3D){
            .width = Viewport.Width,
            .height = Viewport.Height,
            .depth = 1,
        });
    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Framebuffer,
        .renderArea =
            (VkRect2D){
                {
                    Viewport.X,
                    Viewport.Y,
                },
                {
                    Viewport.Z,
                    Viewport.W,
                },
            },
        .renderPass = RenderPass,
        .clearValueCount = AttachmentCount,
        .pClearValues = ClearValues,
    };
    Device->CmdBeginRenderPass(
        CommandBuffer,
        &RenderPassBeginInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    /* Set dynamic states. */

    Device->CmdSetViewport(
        CommandBuffer,
        0,
        1,
        &(VkViewport){
            .x = (float)Viewport.X,
            .y = (float)Viewport.Y,
            .width = (float)Viewport.Width,
            .height = (float)Viewport.Height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

    Device->CmdSetScissor(
        CommandBuffer,
        0,
        1,
        &(VkRect2D){
            .offset.x = Viewport.X,
            .offset.y = Viewport.Y,
            .extent.width = Viewport.Width,
            .extent.height = Viewport.Height,
        });

    Rr_GraphicsPipeline *GraphicsPipeline = NULL;
    Rr_DescriptorsState DescriptorsState = { 0 };

    for(Rr_NodeFunction *Function = Node->Encoded.EncodedFirst;
        Function != NULL;
        Function = Function->Next)
    {
        switch(Function->Type)
        {
            case RR_NODE_FUNCTION_TYPE_DRAW:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    &Frame->DescriptorAllocator,
                    GraphicsPipeline->Layout,
                    Device,
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawArgs *Args = (Rr_DrawArgs *)Function->Args;
                Device->CmdDraw(
                    CommandBuffer,
                    Args->VertexCount,
                    Args->InstanceCount,
                    Args->FirstVertex,
                    Args->FirstInstance);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DRAW_INDEXED:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    &Frame->DescriptorAllocator,
                    GraphicsPipeline->Layout,
                    Device,
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawIndexedArgs *Args = (Rr_DrawIndexedArgs *)Function->Args;
                Device->CmdDrawIndexed(
                    CommandBuffer,
                    Args->IndexCount,
                    Args->InstanceCount,
                    Args->FirstIndex,
                    Args->VertexOffset,
                    Args->FirstInstance);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER:
            {
                Rr_BindIndexBufferArgs *Args = Function->Args;
                Device->CmdBindIndexBuffer(
                    CommandBuffer,
                    Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                    Args->Offset,
                    Args->Type);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER:
            {
                Rr_BindBufferArgs *Args = Function->Args;
                Device->CmdBindVertexBuffers(
                    CommandBuffer,
                    Args->Slot,
                    1,
                    &Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                    &(VkDeviceSize){ Args->Offset });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE:
            {
                GraphicsPipeline = *(Rr_GraphicsPipeline **)Function->Args;
                Device->CmdBindPipeline(
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GraphicsPipeline->Handle);
                Rr_InvalidateDescriptorState(
                    &DescriptorsState,
                    GraphicsPipeline->Layout);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_SET_VIEWPORT:
            {
                Rr_Vec4 *Viewport = Function->Args;
                Device->CmdSetViewport(
                    CommandBuffer,
                    0,
                    1,
                    &(VkViewport){
                        .x = Viewport->X,
                        .y = Viewport->Y,
                        .width = Viewport->Width,
                        .height = Viewport->Height,
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_SET_SCISSOR:
            {
                Rr_IntVec4 *Scissor = Function->Args;
                Device->CmdSetScissor(
                    CommandBuffer,
                    0,
                    1,
                    &(VkRect2D){
                        .offset.x = Scissor->X,
                        .offset.y = Scissor->Y,
                        .extent.width = Scissor->Width,
                        .extent.height = Scissor->Height,
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_SAMPLER:
            {
                Rr_BindSamplerArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_SAMPLER,
                        .Sampler = Args->Sampler->Handle,
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE:
            {
                Rr_BindSampledImageArgs *Args = Function->Args;
                VkImageView ImageView =
                    Rr_GetGraphImage(Graph, Args->ImageHandle)->View;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE,
                        .Image =
                            {
                                .View = ImageView,
                                .Layout = Args->Layout,
                            },
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER:
            {
                Rr_BindCombinedImageSamplerArgs *Args = Function->Args;
                VkImageView ImageView =
                    Rr_GetGraphImage(Graph, Args->ImageHandle)->View;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                        .Image =
                            {
                                .View = ImageView,
                                .Sampler = Args->Sampler->Handle,
                                .Layout = Args->Layout,
                            },
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER:
            {
                Rr_BindUniformBufferArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
                        .Buffer =
                            {
                                .Handle = Rr_GetGraphBuffer(
                                              Graph,
                                              Args->BufferHandle)
                                              ->Handle,
                                .Size = Args->Size,
                                .Offset = Args->Offset,
                            },
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER:
            {
                Rr_BindStorageBufferArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER,
                        .Buffer =
                            {
                                .Handle = Rr_GetGraphBuffer(
                                              Graph,
                                              Args->BufferHandle)
                                              ->Handle,
                                .Size = Args->Size,
                                .Offset = Args->Offset,
                            },
                    });
            }
            break;
            default:
            {
            }
            break;
        }
    }

    Device->CmdEndRenderPass(CommandBuffer);

    Rr_DestroyScratch(Scratch);
}

Rr_GraphNode *Rr_AddGraphNode(
    Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name)
{
    Rr_GraphNode *GraphNode = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphNode));
    GraphNode->Type = Type;
    GraphNode->Name = Name;
    GraphNode->OriginalIndex = Frame->Graph.Nodes.Count;
    GraphNode->Graph = &Frame->Graph;

    RR_RESERVE_SLICE(&GraphNode->Dependencies, 2, Frame->Arena);

    *RR_PUSH_SLICE(&Frame->Graph.Nodes, Frame->Arena) = GraphNode;

    return GraphNode;
}

static inline bool Rr_AddNodeDependency(
    Rr_GraphNode *Node,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State)
{
    for(size_t Index = 0; Index < Node->Dependencies.Count; ++Index)
    {
        Rr_NodeDependency *Dependency = Node->Dependencies.Data + Index;

        if(Dependency->Handle.Values.Index == Handle->Values.Index)
        {
            if(RR_HAS_BIT(State->AccessMask, AllVulkanWrites))
            {
                goto CantWriteWrite;
            }
            else
            {
                if(RR_HAS_BIT(Dependency->State.AccessMask, AllVulkanWrites))
                {
                    goto CantReadWrite;
                }
                else
                {
                    /* Multiple reads might be from different stages. */

                    Dependency->State.StageMask |= State->StageMask;
                    Dependency->State.AccessMask |= State->AccessMask;

                    return true;
                }
            }
        }
    }

    Rr_Graph *Graph = Node->Graph;
    Rr_Arena *Arena = Node->Graph->Arena;

    Rr_GraphHandle CurrentHandle = *Handle;

    Rr_GraphNode **NodeInMap =
        RR_UPSERT(&Graph->ResourceWriteToNode, Handle->Hash, Arena);

    /* Treat any image read as a write for now due to layout transitions. */

    if(State->Specific.Layout != VK_IMAGE_LAYOUT_UNDEFINED ||
       RR_HAS_BIT(State->AccessMask, AllVulkanWrites))
    {
        if(*NodeInMap == NULL)
        {
            size_t NewCapacity =
                RR_MAX(Handle->Values.Index + 1, Graph->RootNodes.Capacity);
            RR_RESERVE_SLICE(&Graph->RootNodes, NewCapacity, Arena);
            Graph->RootNodes.Count = NewCapacity;
            Graph->RootNodes.Data[Handle->Values.Index] = Node;

            Handle->Values.Generation++;

            *NodeInMap = Node;
        }
        else
        {
            goto OtherNodeWrites;
        }
    }

    *RR_PUSH_SLICE(&Node->Dependencies, Arena) = (Rr_NodeDependency){
        .State = *State,
        .Handle = CurrentHandle,
    };

    return true;

CantWriteWrite:

    RR_LOG(
        "Node \"%s\": already writing to the versioned resource!",
        Node->Name);

    return false;

CantReadWrite:

    RR_LOG(
        "Node \"%s\": trying to read and write a versioned resource at the "
        "same time!",
        Node->Name);

    return false;

OtherNodeWrites:

    RR_LOG(
        "Node \"%s\": another node already writes to the versioned resource!",
        Node->Name);

    return false;
}

static void Rr_CreateGraphAdjacencyList(
    Rr_Graph *Graph,
    Rr_IndexSlice *AdjacencyList,
    Rr_Arena *Arena)
{
    for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    {
        Rr_GraphNode *Node = Graph->Nodes.Data[Index];

        for(size_t DepIndex = 0; DepIndex < Node->Dependencies.Count;
            ++DepIndex)
        {
            Rr_NodeDependency *Dependency = Node->Dependencies.Data + DepIndex;

            /* Artifical "read-before-write" dependency. */

            Rr_GraphNode *Writer = RR_UPSERT_DEREF(
                &Graph->ResourceWriteToNode,
                Dependency->Handle.Hash,
                Arena);
            if(Writer != NULL && Writer != Node)
            {
                *RR_PUSH_SLICE(&AdjacencyList[Writer->OriginalIndex], Arena) =
                    Node->OriginalIndex;
            }

            /* If Generation is greater than zero
             * it means we should lookup another node that
             * produces that state of the resource and add
             * that node as a dependency. */

            if(Dependency->Handle.Values.Generation > 0)
            {
                Rr_GraphHandle Handle = Dependency->Handle;
                Handle.Values.Generation--;
                Rr_GraphNode *Producer = RR_UPSERT_DEREF(
                    &Graph->ResourceWriteToNode,
                    Handle.Hash,
                    Arena);
                if(Producer != NULL)
                {
                    *RR_PUSH_SLICE(&AdjacencyList[Index], Arena) =
                        Producer->OriginalIndex;
                }
                else
                {
                    RR_ABORT("Failed to find resource producer!");
                }
            }
        }
    }
}

static void Rr_SortGraph(
    size_t CurrentNodeIndex,
    Rr_IndexSlice *AdjacencyList,
    int *State,
    Rr_GraphNode **Nodes,
    Rr_NodeSlice *Out)
{
    static const int VisitedBit = 1;
    static const int OnStackBit = 2;

    if(RR_HAS_BIT(State[CurrentNodeIndex], VisitedBit))
    {
        if(RR_HAS_BIT(State[CurrentNodeIndex], OnStackBit))
        {
            RR_ABORT(
                "Cyclic graph detected on node \"%s\"!",
                Nodes[CurrentNodeIndex]->Name);
        }

        return;
    }

    State[CurrentNodeIndex] |= VisitedBit;
    State[CurrentNodeIndex] |= OnStackBit;

    Rr_IndexSlice *Dependencies = &AdjacencyList[CurrentNodeIndex];
    for(size_t Index = 0; Index < Dependencies->Count; ++Index)
    {
        Rr_SortGraph(
            Dependencies->Data[Index],
            AdjacencyList,
            State,
            Nodes,
            Out);
    }

    *RR_PUSH_SLICE(Out, NULL) = Nodes[CurrentNodeIndex];

    State[CurrentNodeIndex] &= ~OnStackBit;
}

static void Rr_PrintAdjacencyList(
    Rr_GraphNode **Nodes,
    Rr_IndexSlice *AdjacencyList,
    size_t Count)
{
    for(size_t Index = 0; Index < Count; ++Index)
    {
        Rr_IndexSlice *Deps = AdjacencyList + Index;
        for(size_t DepIndex = 0; DepIndex < Deps->Count; ++DepIndex)
        {
            RR_LOG(
                "Node \"%s\" depends on node \"%s\"",
                Nodes[Index]->Name,
                Nodes[Deps->Data[DepIndex]]->Name);
        }
    }
}

static void Rr_ProcessGraphNodes(
    Rr_Graph *Graph,
    Rr_NodeSlice *SortedNodes,
    Rr_Arena *Arena)
{
    if(Graph->Nodes.Count == 0)
    {
        RR_LOG("Graph doesn't contain any nodes!");

        return;
    }

    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    /* Adjacency list maps a node to a set of nodes that must
     * be executed before it. */

    Rr_IndexSlice *AdjacencyList =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, Rr_IndexSlice, Graph->Nodes.Count);
    Rr_CreateGraphAdjacencyList(Graph, AdjacencyList, Scratch.Arena);

    // Rr_PrintAdjacencyList(Graph->Nodes.Data, AdjacencyList,
    // Graph->Nodes.Count);

    /* Topological sort. */

    int *SortState = RR_ALLOC(Scratch.Arena, sizeof(int) * Graph->Nodes.Count);
    for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    {
        Rr_GraphNode *Node = Graph->Nodes.Data[Index];
        if(Node != NULL)
        {
            Rr_SortGraph(
                Node->OriginalIndex,
                AdjacencyList,
                SortState,
                Graph->Nodes.Data,
                SortedNodes);
        }
    }

    /* Longest path search will determine dependency level for each node.
     * Nodes within same dependency level are meant to be batched together. */

    Rr_GraphNode **Reversed =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, Rr_GraphNode *, SortedNodes->Count);
    for(size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Reversed[Index] = SortedNodes->Data[SortedNodes->Count - 1 - Index];
    }

    for(size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Rr_GraphNode *Node = Reversed[Index];
        size_t OriginalIndex = Node->OriginalIndex;
        for(size_t DepIndex = 0; DepIndex < AdjacencyList[OriginalIndex].Count;
            ++DepIndex)
        {
            size_t DepNodeIndex = AdjacencyList[OriginalIndex].Data[DepIndex];
            Graph->Nodes.Data[DepNodeIndex]->DependencyLevel = RR_MAX(
                Graph->Nodes.Data[DepNodeIndex]->DependencyLevel,
                Graph->Nodes.Data[OriginalIndex]->DependencyLevel + 1);
        }
    }

    Rr_DestroyScratch(Scratch);
}

static void Rr_ExecuteGraphNode(
    Rr_Renderer *Renderer,
    Rr_Graph *Graph,
    Rr_GraphNode *Node,
    VkCommandBuffer CommandBuffer,
    VkCommandBuffer PresentCommandBuffer)
{
    switch(Node->Type)
    {
        case RR_GRAPH_NODE_TYPE_COMPUTE:
        {
            Rr_ComputeNode *ComputeNode = &Node->Union.Compute;
            Rr_ExecuteComputeNode(Renderer, Graph, ComputeNode, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_GRAPHICS:
        {
            Rr_GraphicsNode *GraphicsNode = &Node->Union.Graphics;
            Rr_ExecuteGraphicsNode(
                Renderer,
                Graph,
                GraphicsNode,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_PRESENT:
        {
            Rr_PresentNode *PresentNode = &Node->Union.Present;
            Rr_ExecutePresentNode(Renderer, PresentNode, PresentCommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_BLIT:
        {
            Rr_BlitNode *BlitNode = &Node->Union.Blit;
            Rr_ExecuteBlitNode(Renderer, Graph, BlitNode, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_TRANSFER:
        {
            Rr_TransferNode *TransferNode = &Node->Union.Transfer;
            Rr_ExecuteTransferNode(
                Renderer,
                Graph,
                TransferNode,
                CommandBuffer);
        }
        break;
        default:
        {
            RR_ABORT("Unsupported node type!");
        }
        break;
    }
}

static void Rr_ApplyBarrierBatch(
    Rr_Renderer *Renderer,
    Rr_BarrierBatch *Barrier,
    VkCommandBuffer CommandBuffer,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Device *Device = &Renderer->Device;

    size_t MaxPossibleBarriers =
        Barrier->BufferBarriers.Count + Barrier->ImageBarriers.Count;

    if(MaxPossibleBarriers == 0)
    {
        return;
    }

    const VkPipelineStageFlags AllEarlyStages =
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    VkPipelineStageFlags SrcStageMaskEarly = 0;
    VkPipelineStageFlags DstStageMaskEarly = 0;
    RR_SLICE(VkBufferMemoryBarrier) BufferBarriersEarly = { 0 };
    RR_RESERVE_SLICE(
        &BufferBarriersEarly,
        Barrier->BufferBarriers.Count,
        Scratch.Arena);
    RR_SLICE(VkImageMemoryBarrier) ImageBarriersEarly = { 0 };
    RR_RESERVE_SLICE(
        &ImageBarriersEarly,
        Barrier->ImageBarriers.Count,
        Scratch.Arena);

    VkPipelineStageFlags SrcStageMask = 0;
    VkPipelineStageFlags DstStageMask = 0;
    RR_SLICE(VkBufferMemoryBarrier) BufferBarriers = { 0 };
    RR_RESERVE_SLICE(
        &BufferBarriers,
        Barrier->BufferBarriers.Count,
        Scratch.Arena);
    RR_SLICE(VkImageMemoryBarrier) ImageBarriers = { 0 };
    RR_RESERVE_SLICE(
        &ImageBarriers,
        Barrier->ImageBarriers.Count,
        Scratch.Arena);

    for(size_t Index = 0; Index < Barrier->BufferBarriers.Count; ++Index)
    {
        Rr_BufferMemoryBarrier *BufferBarrier =
            Barrier->BufferBarriers.Data + Index;

        RR_SLICE(VkBufferMemoryBarrier) * BarriersSlice;
        if(BufferBarrier->DstStageMask <= AllEarlyStages)
        {
            SrcStageMaskEarly |= BufferBarrier->SrcStageMask;
            DstStageMaskEarly |= BufferBarrier->DstStageMask;
            BarriersSlice = (void *)&BufferBarriersEarly;
        }
        else
        {
            SrcStageMask |= BufferBarrier->SrcStageMask;
            DstStageMask |= BufferBarrier->DstStageMask;
            BarriersSlice = (void *)&BufferBarriers;
        }

        *RR_PUSH_SLICE(BarriersSlice, Scratch.Arena) = (VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .buffer = BufferBarrier->Buffer,
            .srcAccessMask = BufferBarrier->SrcAccessMask,
            .dstAccessMask = BufferBarrier->DstAccessMask,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };

        Rr_SyncState *BufferState = Rr_GetSynchronizationState(
            Renderer,
            (Rr_MapKey)BufferBarrier->Buffer);
        *BufferState = (Rr_SyncState){
            .StageMask = BufferBarrier->DstStageMask,
            .AccessMask = BufferBarrier->DstAccessMask,
        };
    }

    for(size_t Index = 0; Index < Barrier->ImageBarriers.Count; ++Index)
    {
        Rr_ImageMemoryBarrier *ImageBarrier =
            Barrier->ImageBarriers.Data + Index;

        RR_SLICE(VkImageMemoryBarrier) * BarriersSlice;
        if(ImageBarrier->DstStageMask <= AllEarlyStages)
        {
            SrcStageMaskEarly |= ImageBarrier->SrcStageMask;
            DstStageMaskEarly |= ImageBarrier->DstStageMask;
            BarriersSlice = (void *)&ImageBarriersEarly;
        }
        else
        {
            SrcStageMask |= ImageBarrier->SrcStageMask;
            DstStageMask |= ImageBarrier->DstStageMask;
            BarriersSlice = (void *)&ImageBarriers;
        }

        *RR_PUSH_SLICE(BarriersSlice, Scratch.Arena) = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = ImageBarrier->Image,
            .srcAccessMask = ImageBarrier->SrcAccessMask,
            .dstAccessMask = ImageBarrier->DstAccessMask,
            .oldLayout = ImageBarrier->OldLayout,
            .newLayout = ImageBarrier->NewLayout,
            .subresourceRange = ImageBarrier->SubresourceRange,
        };

        Rr_SyncState *ImageState = Rr_GetSynchronizationState(
            Renderer,
            (Rr_MapKey)ImageBarrier->Image);

        *ImageState = (Rr_SyncState){
            .StageMask = ImageBarrier->DstStageMask,
            .AccessMask = ImageBarrier->DstAccessMask,
            .Specific.Layout = ImageBarrier->NewLayout,
        };
    }

    if(BufferBarriersEarly.Count > 0 || ImageBarriersEarly.Count > 0)
    {
        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcStageMaskEarly,
            DstStageMaskEarly,
            0,
            0,
            NULL,
            BufferBarriersEarly.Count,
            BufferBarriersEarly.Data,
            ImageBarriersEarly.Count,
            ImageBarriersEarly.Data);
    }

    if(BufferBarriers.Count > 0 || ImageBarriers.Count > 0)
    {
        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcStageMask,
            DstStageMask,
            0,
            0,
            NULL,
            BufferBarriers.Count,
            BufferBarriers.Data,
            ImageBarriers.Count,
            ImageBarriers.Data);
    }

    Barrier->ImageBarriers.Count = 0;
    Barrier->BufferBarriers.Count = 0;
    Barrier->VulkanHandleToBarrier = NULL;

    Rr_DestroyScratch(Scratch);
}

void Rr_ExecuteGraph(Rr_Renderer *Renderer, Rr_Graph *Graph, Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_NodeSlice SortedNodes = { 0 };
    RR_RESERVE_SLICE(&SortedNodes, Graph->Nodes.Count, Scratch.Arena);

    Rr_ProcessGraphNodes(Graph, &SortedNodes, Scratch.Arena);

    /* Resolve all referenced resources. */

    for(size_t Index = 0; Index < Graph->Resources.Count; ++Index)
    {
        Rr_GraphResource *Resource = Graph->Resources.Data + Index;
        if(Resource->IsImage)
        {
            Resource->Allocated =
                Rr_GetCurrentAllocatedImage(Renderer, Resource->Container);
        }
        else
        {
            Resource->Allocated =
                Rr_GetCurrentAllocatedBuffer(Renderer, Resource->Container);
        }
    }

    size_t DependencyLevel = SortedNodes.Data[0]->DependencyLevel;

    Rr_BarrierBatch BarrierBatch = { 0 };

    size_t BatchStartIndex = 0;
    size_t BatchSize = 0;

    for(size_t Index = 0; Index < SortedNodes.Count; ++Index)
    {
        Rr_GraphNode *Node = SortedNodes.Data[Index];
        DependencyLevel = Node->DependencyLevel;

        for(size_t DepIndex = 0; DepIndex < Node->Dependencies.Count;
            ++DepIndex)
        {
            Rr_NodeDependency *Dependency = Node->Dependencies.Data + DepIndex;
            Rr_SyncState *State = &Dependency->State;

            if(State->Specific.Layout != 0)
            {
                /* Image Synchronization */

                Rr_AllocatedImage *AllocatedImage =
                    Rr_GetGraphImage(Graph, Dependency->Handle);
                VkImage Image = AllocatedImage->Handle;

                Rr_SyncState *PrevState =
                    Rr_GetSynchronizationState(Renderer, (Rr_MapKey)Image);

                /* If reading again, just make sure the memory is "available" to
                 * this memory domain AND the image is in the same layout. */

                bool IsReadingNow =
                    RR_HAS_BIT(State->AccessMask, AllVulkanWrites) == 0;
                bool WasReadingBefore =
                    RR_HAS_BIT(PrevState->AccessMask, AllVulkanWrites) == 0;
                bool IsSameLayout =
                    State->Specific.Layout == PrevState->Specific.Layout;
                if(IsReadingNow && WasReadingBefore && IsSameLayout)
                {
                    bool IncludesPreviousAccessMask =
                        (PrevState->AccessMask & State->AccessMask) ==
                        State->AccessMask;
                    if(IncludesPreviousAccessMask)
                    {
                        /* Skip this barrier! */

                        continue;
                    }
                }

                Rr_ImageMemoryBarrier **ImageBarrierRef = RR_UPSERT(
                    &BarrierBatch.VulkanHandleToBarrier,
                    Image,
                    Scratch.Arena);
                Rr_ImageMemoryBarrier *ImageBarrier = *ImageBarrierRef;
                if(ImageBarrier == NULL)
                {
                    *ImageBarrierRef = RR_PUSH_SLICE(
                        &BarrierBatch.ImageBarriers,
                        Scratch.Arena);
                    ImageBarrier = *ImageBarrierRef;
                    *ImageBarrier = (Rr_ImageMemoryBarrier){
                        .SrcStageMask = PrevState->StageMask != 0
                                            ? PrevState->StageMask
                                            : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        .DstStageMask = State->StageMask,
                        .Image = Image,
                        .SrcAccessMask = PrevState->AccessMask,
                        .DstAccessMask = State->AccessMask,
                        .OldLayout = PrevState->Specific.Layout,
                        .NewLayout = State->Specific.Layout,
                        .SubresourceRange =
                            (VkImageSubresourceRange){
                                .aspectMask =
                                    AllocatedImage->Container->AspectFlags,
                                .baseMipLevel = 0,
                                .levelCount = VK_REMAINING_MIP_LEVELS,
                                .baseArrayLayer = 0,
                                .layerCount = VK_REMAINING_ARRAY_LAYERS,
                            },
                    };
                }
                else
                {
                    RR_ABORT("Multiple image layout transitions!");
                }
            }
            else
            {
                /* Buffer Synchronization */

                Rr_AllocatedBuffer *AllocatedBuffer =
                    Rr_GetGraphBuffer(Graph, Dependency->Handle);
                VkBuffer Buffer = AllocatedBuffer->Handle;

                Rr_SyncState *PrevState =
                    Rr_GetSynchronizationState(Renderer, (Rr_MapKey)Buffer);

                /* If reading again, just make sure the memory is "available" to
                 * this memory domain. */

                bool IsReadingNow =
                    RR_HAS_BIT(State->AccessMask, AllVulkanWrites) == 0;
                bool WasReadingBefore =
                    RR_HAS_BIT(PrevState->AccessMask, AllVulkanWrites) == 0;
                if(IsReadingNow && WasReadingBefore)
                {
                    bool IncludesPreviousAccessMask =
                        (PrevState->AccessMask & State->AccessMask) ==
                        State->AccessMask;
                    if(IncludesPreviousAccessMask)
                    {
                        /* Skip this barrier! */

                        continue;
                    }
                }

                Rr_BufferMemoryBarrier **BufferBarrierRef = RR_UPSERT(
                    &BarrierBatch.VulkanHandleToBarrier,
                    Buffer,
                    Scratch.Arena);
                Rr_BufferMemoryBarrier *BufferBarrier = *BufferBarrierRef;
                if(BufferBarrier == NULL)
                {
                    *BufferBarrierRef = RR_PUSH_SLICE(
                        &BarrierBatch.BufferBarriers,
                        Scratch.Arena);
                    BufferBarrier = *BufferBarrierRef;
                    *BufferBarrier = (Rr_BufferMemoryBarrier){
                        .SrcStageMask = PrevState->StageMask != 0
                                            ? PrevState->StageMask
                                            : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        .DstStageMask = State->StageMask,
                        .Buffer = Buffer,
                        .SrcAccessMask = PrevState->AccessMask,
                        .DstAccessMask = State->AccessMask,
                        .Offset = 0,
                        .Size = VK_WHOLE_SIZE,
                    };
                }
                else
                {
                    BufferBarrier->SrcStageMask |= PrevState->StageMask;
                    BufferBarrier->DstStageMask |= State->StageMask;
                    BufferBarrier->SrcAccessMask |= PrevState->AccessMask;
                    BufferBarrier->DstAccessMask |= State->AccessMask;
                }
            }
        }

        BatchSize++;

        bool LastNode = false;
        bool LastNodeThisLevel = false;
        if(Index + 1 < SortedNodes.Count)
        {
            LastNodeThisLevel =
                SortedNodes.Data[Index + 1]->DependencyLevel != DependencyLevel;
        }
        else
        {
            LastNode = true;
        }
        if(LastNode || LastNodeThisLevel)
        {
            /* Execute current batch now! */

            Rr_ApplyBarrierBatch(
                Renderer,
                &BarrierBatch,
                Frame->MainCommandBuffer,
                Scratch.Arena);

            for(size_t NodeIndex = BatchStartIndex;
                NodeIndex < BatchStartIndex + BatchSize;
                ++NodeIndex)
            {
                Rr_ExecuteGraphNode(
                    Renderer,
                    Graph,
                    SortedNodes.Data[NodeIndex],
                    Frame->MainCommandBuffer,
                    Frame->PresentCommandBuffer);
            }

            BatchStartIndex = Index + 1;
            BatchSize = 0;
        }
    }

    Rr_DestroyScratch(Scratch);
}

static inline Rr_GraphImage *Rr_GetGraphHandle(
    Rr_Graph *Graph,
    void *Container,
    bool IsImage)
{
    assert(Container != NULL);

    Rr_GraphHandle **GraphHandle =
        RR_UPSERT(&Graph->Handles, (Rr_MapKey)Container, Graph->Arena);
    if(*GraphHandle == NULL)
    {
        Rr_GraphImage Handle = {
            .Values.Index = Graph->Resources.Count,
        };
        *RR_PUSH_SLICE(&Graph->Resources, Graph->Arena) = (Rr_GraphResource){
            .Container = Container,
            .IsImage = IsImage,
        };
        *GraphHandle = RR_ALLOC_TYPE(Graph->Arena, Rr_GraphHandle);
        **GraphHandle = Handle;
    }

    return *GraphHandle;
}

static inline Rr_GraphBuffer *Rr_GetGraphBufferHandle(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer)
{
    return Rr_GetGraphHandle(Graph, Buffer, false);
}

static inline Rr_GraphBuffer *Rr_GetGraphImageHandle(
    Rr_Graph *Graph,
    Rr_Image *Image)
{
    return Rr_GetGraphHandle(Graph, Image, true);
}

Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_Image *Image,
    Rr_Sampler *Sampler,
    Rr_Vec4 ColorClear,
    Rr_PresentMode Mode)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_PRESENT, Name);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(&Frame->Graph, Image);

    Rr_PresentNode *PresentNode = &GraphNode->Union.Present;
    PresentNode->Mode = Mode;
    PresentNode->ImageHandle = *ImageHandle;
    PresentNode->Sampler = Sampler;
    PresentNode->ColorClear = ColorClear;

    /*Rr_GraphImageHandle SwapchainImageHandle = Rr_RegisterGraphImage(App,
     * Rr_GetSwapchainImage(App));*/
    /**/
    /*Rr_AddNodeDependency(*/
    /*    GraphNode,*/
    /*    &SwapchainImageHandle,*/
    /*    &(Rr_GenericSync){*/
    /*        .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,*/
    /*        .AccessMask = VK_ACCESS_SHADER_WRITE_BIT |
     * VK_ACCESS_SHADER_READ_BIT,*/
    /*        .Specific.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,*/
    /*    });*/

    Rr_AddNodeDependency(
        GraphNode,
        ImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .Specific.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

    return GraphNode;
}

Rr_GraphNode *Rr_AddTransferNode(Rr_App *App, const char *Name)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_TRANSFER, Name);

    Rr_TransferNode *TransferNode = &GraphNode->Union.Transfer;
    RR_RESERVE_SLICE(&TransferNode->Transfers, 2, Frame->Arena);

    return GraphNode;
}

void Rr_TransferBufferData(
    Rr_App *App,
    Rr_GraphNode *Node,
    size_t Size,
    Rr_Buffer *SrcBuffer,
    size_t SrcOffset,
    Rr_Buffer *DstBuffer,
    size_t DstOffset)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_TransferNode *TransferNode = &Node->Union.Transfer;

    Rr_GraphBuffer *SrcBufferHandle =
        Rr_GetGraphBufferHandle(&Frame->Graph, SrcBuffer);
    Rr_GraphBuffer *DstBufferHandle =
        Rr_GetGraphBufferHandle(&Frame->Graph, DstBuffer);

    *RR_PUSH_SLICE(&TransferNode->Transfers, Frame->Arena) = (Rr_Transfer){
        .Size = Size,
        .SrcOffset = SrcOffset,
        .SrcBuffer = *SrcBufferHandle,
        .DstOffset = DstOffset,
        .DstBuffer = *DstBufferHandle,
    };

    Rr_AddNodeDependency(
        Node,
        SrcBufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        });

    Rr_AddNodeDependency(
        Node,
        DstBufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        });
}

Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_Image *SrcImage,
    Rr_Image *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_BlitMode Mode)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_BLIT, Name);

    Rr_GraphImage *SrcImageHandle =
        Rr_GetGraphImageHandle(&Frame->Graph, SrcImage);
    Rr_GraphImage *DstImageHandle =
        Rr_GetGraphImageHandle(&Frame->Graph, DstImage);

    Rr_BlitNode *BlitNode = &GraphNode->Union.Blit;
    *BlitNode = (Rr_BlitNode){
        .SrcImageHandle = *SrcImageHandle,
        .DstImageHandle = *DstImageHandle,
        .SrcRect = SrcRect,
        .DstRect = DstRect,
    };

    switch(Mode)
    {
        case RR_BLIT_MODE_COLOR:
        {
            BlitNode->AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        break;
        case RR_BLIT_MODE_DEPTH:
        {
            BlitNode->AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        break;
        default:
        {
            RR_ABORT("Unsupported blit mode!");
        }
        break;
    }

    Rr_AddNodeDependency(
        GraphNode,
        SrcImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Specific.Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        });

    Rr_AddNodeDependency(
        GraphNode,
        DstImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Specific.Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });

    return GraphNode;
}

Rr_GraphNode *Rr_AddComputeNode(Rr_App *App, const char *Name)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_COMPUTE, Name);

    Rr_ComputeNode *ComputeNode = &GraphNode->Union.Compute;

    ComputeNode->Encoded.Encoded =
        RR_ALLOC(Frame->Arena, sizeof(Rr_NodeFunction));
    ComputeNode->Encoded.EncodedFirst = ComputeNode->Encoded.Encoded;

    return GraphNode;
}

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    size_t ColorTargetCount,
    Rr_ColorTarget *ColorTargets,
    Rr_Image **ColorImages,
    Rr_DepthTarget *DepthTarget,
    Rr_Image *DepthImage)
{
    assert(ColorTargetCount > 0 || DepthTarget != NULL);

    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_GRAPHICS, Name);

    // Rr_GraphImage *SrcImageHandle =
    //     Rr_GetGraphImageHandle(&Frame->Graph, SrcImage);
    // Rr_GraphImage *DstImageHandle =
    //     Rr_GetGraphImageHandle(&Frame->Graph, DstImage);

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.Graphics;
    if(ColorTargetCount > 0)
    {
        GraphicsNode->ColorTargetCount = ColorTargetCount;
        GraphicsNode->ColorTargets =
            RR_ALLOC_TYPE_COUNT(Frame->Arena, Rr_ColorTarget, ColorTargetCount);
        GraphicsNode->ColorImages =
            RR_ALLOC_TYPE_COUNT(Frame->Arena, Rr_GraphImage, ColorTargetCount);

        for(size_t Index = 0; Index < ColorTargetCount; ++Index)
        {
            Rr_GraphImage *ColorImageHandle =
                Rr_GetGraphImageHandle(&Frame->Graph, ColorImages[Index]);

            GraphicsNode->ColorTargets[Index] = ColorTargets[Index];
            GraphicsNode->ColorImages[Index] = *ColorImageHandle;

            VkAccessFlags AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            if(ColorTargets[Index].LoadOp == RR_LOAD_OP_LOAD)
            {
                AccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            }
            Rr_AddNodeDependency(
                GraphNode,
                ColorImageHandle,
                &(Rr_SyncState){
                    .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .AccessMask = AccessMask,
                    .Specific.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });
        }
    }
    if(DepthTarget != NULL)
    {
        Rr_GraphImage *DepthImageHandle =
            Rr_GetGraphImageHandle(&Frame->Graph, DepthImage);

        GraphicsNode->DepthTarget = RR_ALLOC_TYPE(Frame->Arena, Rr_DepthTarget);
        *GraphicsNode->DepthTarget = *DepthTarget;
        GraphicsNode->DepthImage = *DepthImageHandle;

        VkAccessFlags AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if(DepthTarget->LoadOp == RR_LOAD_OP_LOAD)
        {
            AccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        Rr_AddNodeDependency(
            GraphNode,
            DepthImageHandle,
            &(Rr_SyncState){
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .AccessMask = AccessMask,
                .Specific.Layout =
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            });
    }

    GraphicsNode->Encoded.Encoded =
        RR_ALLOC(Frame->Arena, sizeof(Rr_NodeFunction));
    GraphicsNode->Encoded.EncodedFirst = GraphicsNode->Encoded.Encoded;

    return GraphNode;
}

#define RR_NODE_ENCODE(FunctionType, ArgsType)                         \
    Rr_Arena *Arena = Node->Graph->Arena;                              \
    Rr_Encoded *Encoded = (Rr_Encoded *)&Node->Union;                  \
    Encoded->Encoded->Next = RR_ALLOC(Arena, sizeof(Rr_NodeFunction)); \
    Encoded->Encoded = Encoded->Encoded->Next;                         \
    Encoded->Encoded->Type = FunctionType;                             \
    Encoded->Encoded->Args = RR_ALLOC(Arena, sizeof(ArgsType));        \
    *(ArgsType *)Encoded->Encoded->Args

void Rr_BindComputePipeline(
    Rr_GraphNode *Node,
    Rr_ComputePipeline *ComputePipeline)
{
    assert(ComputePipeline != NULL);
    assert(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_COMPUTE_PIPELINE,
        Rr_ComputePipeline *) = ComputePipeline;
}

void Rr_Dispatch(
    Rr_GraphNode *Node,
    uint32_t GroupCountX,
    uint32_t GroupCountY,
    uint32_t GroupCountZ)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE);
    assert(GroupCountX >= 1);
    assert(GroupCountY >= 1);
    assert(GroupCountZ >= 1);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DISPATCH, Rr_DispatchArgs) =
        (Rr_DispatchArgs){

            .GroupCountX = GroupCountX,
            .GroupCountY = GroupCountY,
            .GroupCountZ = GroupCountZ,
        };
}

void Rr_Draw(
    Rr_GraphNode *Node,
    uint32_t VertexCount,
    uint32_t InstanceCount,
    uint32_t FirstVertex,
    uint32_t FirstInstance)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DRAW, Rr_DrawArgs) = (Rr_DrawArgs){
        .VertexCount = VertexCount,
        .InstanceCount = InstanceCount,
        .FirstVertex = FirstVertex,
        .FirstInstance = FirstInstance,
    };
}

void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DRAW_INDEXED, Rr_DrawIndexedArgs) =
        (Rr_DrawIndexedArgs){
            .IndexCount = IndexCount,
            .InstanceCount = InstanceCount,
            .FirstIndex = FirstIndex,
            .VertexOffset = VertexOffset,
            .FirstInstance = FirstInstance,
        };
}

void Rr_BindVertexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint32_t Offset)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
        Rr_BindIndexBufferArgs) = (Rr_BindIndexBufferArgs){
        .BufferHandle = *BufferHandle,
        .Slot = Slot,
        .Offset = Offset,
    };

    Rr_AddNodeDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        });
}

void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint32_t Offset,
    Rr_IndexType Type)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
        Rr_BindIndexBufferArgs) = (Rr_BindIndexBufferArgs){
        .BufferHandle = *BufferHandle,
        .Slot = Slot,
        .Offset = Offset,
        .Type = Rr_GetVulkanIndexType(Type),
    };

    Rr_AddNodeDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_INDEX_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        });
}

void Rr_BindGraphicsPipeline(
    Rr_GraphNode *Node,
    Rr_GraphicsPipeline *GraphicsPipeline)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
        Rr_GraphicsPipeline *) = GraphicsPipeline;
}

void Rr_SetViewport(Rr_GraphNode *Node, Rr_Vec4 Rect)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_SET_VIEWPORT, Rr_Vec4) = Rect;
}

void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntVec4 Rect)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_SET_SCISSOR, Rr_IntVec4) = Rect;
}

void Rr_BindSampler(
    Rr_GraphNode *Node,
    struct Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Sampler != NULL);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_BIND_SAMPLER, Rr_BindSamplerArgs) =
        (Rr_BindSamplerArgs){
            .Sampler = Sampler,
            .Set = Set,
            .Binding = Binding,
        };
}

void Rr_BindSampledImage(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    uint32_t Set,
    uint32_t Binding)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Image);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Image);

    VkImageLayout Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE,
        Rr_BindSampledImageArgs) = (Rr_BindSampledImageArgs){
        .ImageHandle = *ImageHandle,
        .Layout = Layout,
        .Set = Set,
        .Binding = Binding,
    };

    /* @TODO: Stage mask can be infered from pipeline layout. */

    Rr_AddNodeDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .Specific.Layout = Layout,
        });
}

void Rr_BindCombinedImageSampler(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Sampler != NULL);
    assert(Image);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Image);

    VkImageLayout Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER,
        Rr_BindCombinedImageSamplerArgs) = (Rr_BindCombinedImageSamplerArgs){
        .ImageHandle = *ImageHandle,
        .Layout = Layout,
        .Sampler = Sampler,
        .Set = Set,
        .Binding = Binding,
    };

    /* @TODO: Stage mask can be infered from pipeline layout. */

    Rr_AddNodeDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .Specific.Layout = Layout,
        });
}

void Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t Offset,
    uint32_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER,
        Rr_BindUniformBufferArgs) = (Rr_BindUniformBufferArgs){
        .BufferHandle = *BufferHandle,
        .Set = Set,
        .Binding = Binding,
        .Offset = Offset,
        .Size = Size,
    };

    /* @TODO: Proper stage can be infered from pipeline layout. */

    if(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE)
    {

        Rr_AddNodeDependency(
            Node,
            BufferHandle,
            &(Rr_SyncState){
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            });
    }
    else
    {

        Rr_AddNodeDependency(
            Node,
            BufferHandle,
            &(Rr_SyncState){
                .AccessMask = VK_ACCESS_UNIFORM_READ_BIT,
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            });
    }
}

void Rr_BindStorageBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t Offset,
    uint32_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER,
        Rr_BindStorageBufferArgs) = (Rr_BindStorageBufferArgs){
        .BufferHandle = *BufferHandle,
        .Set = Set,
        .Binding = Binding,
        .Offset = Offset,
        .Size = Size,
    };

    /* @TODO: Proper read/write stuff. */

    if(Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE)
    {
        Rr_AddNodeDependency(
            Node,
            BufferHandle,
            &(Rr_SyncState){
                .AccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            });
    }
    else
    {
        Rr_AddNodeDependency(
            Node,
            BufferHandle,
            &(Rr_SyncState){
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            });
    }
}
