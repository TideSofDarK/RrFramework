/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Graph.h"

#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <assert.h>

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

static void Rr_ExecuteTransferNode(
    Rr_Graph *Graph,
    Rr_TransferNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    for (size_t Index = 0; Index < Node->Transfers.Count; ++Index)
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
    Rr_Graph *Graph,
    Rr_BlitNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_AllocatedImage *SrcImage =
        Rr_GetGraphImage(Frame->Graph, Node->SrcImageHandle);
    Rr_AllocatedImage *DstImage =
        Rr_GetGraphImage(Frame->Graph, Node->DstImageHandle);

    if (Rr_ClampBlitRect(&Node->SrcRect, &SrcImage->Container->Extent) &&
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
    Rr_Graph *Graph,
    Rr_ComputeNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_ComputePipeline *Pipeline = NULL;
    Rr_DescriptorsState DescriptorsState = { 0 };

    for (Rr_NodeFunction *Function = Node->Encoded.EncodedFirst;
         Function != NULL;
         Function = Function->Next)
    {
        switch (Function->Type)
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
                assert(Pipeline != NULL);
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
            case RR_NODE_FUNCTION_TYPE_BIND_STORAGE_IMAGE:
            {
                Rr_BindStorageImageArgs *Args = Function->Args;
                VkImageView ImageView =
                    Rr_GetGraphImage(Graph, Args->ImageHandle)->View;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_STORAGE_IMAGE,
                        .Image =
                            {
                                .View = ImageView,
                                .Layout = VK_IMAGE_LAYOUT_GENERAL,
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
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_IntVec4 Viewport = { 0 };
    Viewport.Width = INT32_MAX;
    Viewport.Height = INT32_MAX;

    /* Line up appropriate clear values. */

    size_t AttachmentCount =
        Node->ColorTargetCount + (Node->DepthTarget ? 1 : 0);

    Rr_RenderPassAttachment *Attachments = RR_ALLOC_TYPE_COUNT(
        Scratch.Arena,
        Rr_RenderPassAttachment,
        AttachmentCount);
    VkImageView *ImageViews =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImageView, AttachmentCount);
    VkClearValue *ClearValues =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkClearValue, AttachmentCount);
    for (uint32_t Index = 0; Index < Node->ColorTargetCount; ++Index)
    {
        Rr_ColorTarget *ColorTarget = &Node->ColorTargets[Index];
        VkClearValue *ClearValue = &ClearValues[ColorTarget->Slot];
        memcpy(ClearValue, &ColorTarget->Clear, sizeof(VkClearValue));
        Rr_AllocatedImage *ColorImage =
            Rr_GetGraphImage(Graph, Node->ColorImages[Index]);
        Attachments[ColorTarget->Slot] = (Rr_RenderPassAttachment){
            .LoadOp = ColorTarget->LoadOp,
            .StoreOp = ColorTarget->StoreOp,
            .Format = ColorImage->Container->Format,
        };
        ImageViews[ColorTarget->Slot] = ColorImage->View;

        Viewport.Width = RR_MIN(
            Viewport.Width,
            (int32_t)ColorImage->Container->Extent.width);
        Viewport.Height = RR_MIN(
            Viewport.Height,
            (int32_t)ColorImage->Container->Extent.height);
    }
    if (Node->DepthTarget != NULL)
    {
        size_t DepthIndex = AttachmentCount - 1;
        Rr_DepthTarget *DepthTarget = Node->DepthTarget;
        VkClearValue *ClearValue = &ClearValues[DepthIndex];
        memcpy(ClearValue, &DepthTarget->Clear, sizeof(VkClearValue));
        Rr_AllocatedImage *DepthImage =
            Rr_GetGraphImage(Graph, Node->DepthImage);
        Attachments[DepthIndex] = (Rr_RenderPassAttachment){
            .LoadOp = DepthTarget->LoadOp,
            .StoreOp = DepthTarget->StoreOp,
            .Format = DepthImage->Container->Format,
        };
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
    VkRenderPass RenderPass = Rr_GetVulkanRenderPass(&RenderPassInfo);
    VkFramebuffer Framebuffer = Rr_GetVulkanFramebuffer(
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
        .clearValueCount = (uint32_t)AttachmentCount,
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

    for (Rr_NodeFunction *Function = Node->Encoded.EncodedFirst;
         Function != NULL;
         Function = Function->Next)
    {
        switch (Function->Type)
        {
            case RR_NODE_FUNCTION_TYPE_DRAW:
            {
                assert(GraphicsPipeline != NULL);
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
            case RR_NODE_FUNCTION_TYPE_DRAW_INDIRECT:
            {
                assert(GraphicsPipeline != NULL);
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    &Frame->DescriptorAllocator,
                    GraphicsPipeline->Layout,
                    Device,
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawIndirectArgs *Args =
                    (Rr_DrawIndirectArgs *)Function->Args;
                Device->CmdDrawIndirect(
                    CommandBuffer,
                    Rr_GetGraphBuffer(Graph, Args->BufferHandle)->Handle,
                    Args->Offset,
                    Args->Count,
                    Args->Stride);
            }
            break;
            case RR_NODE_FUNCTION_TYPE_DRAW_INDEXED:
            {
                assert(GraphicsPipeline != NULL);
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
                Rr_Rect *ViewportRect = Function->Args;
                Device->CmdSetViewport(
                    CommandBuffer,
                    0,
                    1,
                    &(VkViewport){
                        .x = ViewportRect->Offset.X,
                        .y = ViewportRect->Offset.Y,
                        .width = ViewportRect->Extent.Width,
                        .height = ViewportRect->Extent.Height,
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                    });
            }
            break;
            case RR_NODE_FUNCTION_TYPE_SET_SCISSOR:
            {
                Rr_IntRect *ScissorRect = Function->Args;

                /* NOTE: VkRect2D is the same as Rr_IntRect. */

                Device->CmdSetScissor(
                    CommandBuffer,
                    0,
                    1,
                    (VkRect2D *)ScissorRect);
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

static void Rr_ExecuteClearColorImageNode(
    Rr_Graph *Graph,
    Rr_ClearColorImageNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_AllocatedImage *ColorImage = Rr_GetGraphImage(Graph, Node->ColorImage);

    Device->CmdClearColorImage(
        CommandBuffer,
        ColorImage->Handle,
        VK_IMAGE_LAYOUT_GENERAL,
        (void *)&Node->ColorClear,
        1,
        &(VkImageSubresourceRange){
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        });
}

static void Rr_ExecuteCopyBufferToImageNode(
    Rr_Graph *Graph,
    Rr_CopyBufferToImageNode *Node,
    VkCommandBuffer CommandBuffer)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_AllocatedImage *Image = Rr_GetGraphImage(Graph, Node->Image);
    Rr_ImageContainer *ImageContainer = Image->Container;

    VkBuffer BufferHandle = Rr_GetGraphBuffer(Graph, Node->Buffer)->Handle;

    VkBufferImageCopy BufferImageCopy = {
        .bufferOffset = Node->BufferOffset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            (VkImageSubresourceLayers){
                .aspectMask = ImageContainer->AspectFlags,
                .mipLevel = Node->MipLevel,
                .baseArrayLayer = Node->BaseLayer,
                .layerCount = Node->LayerCount,
            },
        .imageExtent =
            (VkExtent3D){
                ImageContainer->Extent.width,
                ImageContainer->Extent.height,
                1,
            },
    };

    Device->CmdCopyBufferToImage(
        CommandBuffer,
        BufferHandle,
        Image->Handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &BufferImageCopy);
}

Rr_GraphNode *Rr_AddGraphNode(
    Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name)
{
    Rr_GraphNode *GraphNode = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphNode));
    GraphNode->Type = Type;
    GraphNode->Name = Name;
    GraphNode->OriginalIndex = Frame->Graph->Nodes.Count;
    GraphNode->Graph = Frame->Graph;

    RR_RESERVE_ARRAY(&GraphNode->Dependencies, 2, Frame->Arena);

    *RR_PUSH_INTO_ARRAY(&Frame->Graph->Nodes, Frame->Arena) = GraphNode;

    return GraphNode;
}

static inline bool Rr_AddNodeDependency(
    Rr_GraphNode *Node,
    Rr_GraphHandle *Handle,
    Rr_SyncState *State)
{
    for (size_t Index = 0; Index < Node->Dependencies.Count; ++Index)
    {
        Rr_NodeDependency *Dependency = Node->Dependencies.Data + Index;

        if (Dependency->Handle.Values.Index == Handle->Values.Index)
        {
            if (RR_HAS_BIT(State->AccessMask, RR_VULKAN_WRITES))
            {
                goto CantWriteWrite;
            }
            else
            {
                if (RR_HAS_BIT(Dependency->State.AccessMask, RR_VULKAN_WRITES))
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
    Rr_Arena *Arena = Node->Graph->Frame->Arena;

    if (Handle->Values.Index == Graph->SwapchainImageResourceIndex)
    {
        Node->UsesLateCommandBuffer = true;
    }

    Rr_GraphHandle CurrentHandle = *Handle;

    Rr_GraphNode **NodeInMap =
        RR_GET_MAP_VALUE(&Graph->ResourceWriteToNode, Handle->Hash, Arena);

    /* Treat any image read as a write for now due to layout transitions. */

    if (State->Layout != VK_IMAGE_LAYOUT_UNDEFINED ||
        RR_HAS_BIT(State->AccessMask, RR_VULKAN_WRITES))
    {
        if (*NodeInMap == NULL)
        {
            Handle->Values.Generation++;

            *NodeInMap = Node;
        }
        else
        {
            goto OtherNodeWrites;
        }
    }

    *RR_PUSH_INTO_ARRAY(&Node->Dependencies, Arena) = (Rr_NodeDependency){
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
    Rr_IndexArray *AdjacencyList,
    Rr_Arena *Arena)
{
    for (size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    {
        Rr_GraphNode *Node = Graph->Nodes.Data[Index];

        for (size_t DepIndex = 0; DepIndex < Node->Dependencies.Count;
             ++DepIndex)
        {
            Rr_NodeDependency *Dependency = Node->Dependencies.Data + DepIndex;

            /* Artifical "read-before-write" dependency. */

            Rr_GraphNode *Writer = RR_GET_MAP_VALUE_DEREF(
                &Graph->ResourceWriteToNode,
                Dependency->Handle.Hash,
                Arena);
            if (Writer != NULL && Writer != Node)
            {
                *RR_PUSH_INTO_ARRAY(
                    &AdjacencyList[Writer->OriginalIndex],
                    Arena) = Node->OriginalIndex;
            }

            /* If Generation is greater than zero
             * it means we should lookup another node that
             * produces that state of the resource and add
             * that node as a dependency. */

            if (Dependency->Handle.Values.Generation > 0)
            {
                Rr_GraphHandle Handle = Dependency->Handle;
                Handle.Values.Generation--;
                Rr_GraphNode *Producer = RR_GET_MAP_VALUE_DEREF(
                    &Graph->ResourceWriteToNode,
                    Handle.Hash,
                    Arena);
                if (Producer != NULL)
                {
                    *RR_PUSH_INTO_ARRAY(&AdjacencyList[Index], Arena) =
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
    Rr_IndexArray *AdjacencyList,
    int *State,
    Rr_GraphNode **Nodes,
    Rr_NodeArray *Out)
{
    static const int VisitedBit = 1;
    static const int OnStackBit = 2;

    if (RR_HAS_BIT(State[CurrentNodeIndex], VisitedBit))
    {
        if (RR_HAS_BIT(State[CurrentNodeIndex], OnStackBit))
        {
            RR_ABORT(
                "Cyclic graph detected on node \"%s\"!",
                Nodes[CurrentNodeIndex]->Name);
        }

        return;
    }

    State[CurrentNodeIndex] |= VisitedBit;
    State[CurrentNodeIndex] |= OnStackBit;

    Rr_IndexArray *Dependencies = &AdjacencyList[CurrentNodeIndex];
    for (size_t Index = 0; Index < Dependencies->Count; ++Index)
    {
        Rr_SortGraph(
            Dependencies->Data[Index],
            AdjacencyList,
            State,
            Nodes,
            Out);
    }

    *RR_PUSH_INTO_ARRAY(Out, NULL) = Nodes[CurrentNodeIndex];

    State[CurrentNodeIndex] &= ~OnStackBit;
}

static void Rr_PrintAdjacencyList(
    Rr_GraphNode **Nodes,
    Rr_IndexArray *AdjacencyList,
    size_t Count)
{
    for (size_t Index = 0; Index < Count; ++Index)
    {
        Rr_IndexArray *Deps = AdjacencyList + Index;

        RR_LOG(
            "%s Node \"%s\"",
            Nodes[Index]->UsesLateCommandBuffer ? "late" : "early",
            Nodes[Index]->Name);
        for (size_t DepIndex = 0; DepIndex < Deps->Count; ++DepIndex)
        {
            RR_LOG(
                " depends on node \"%s\"",
                Nodes[Deps->Data[DepIndex]]->Name);
        }
    }
}

static void Rr_ProcessGraphNodes(
    Rr_Graph *Graph,
    Rr_NodeArray *SortedNodes,
    Rr_Arena *Arena)
{
    if (Graph->Nodes.Count == 0)
    {
        RR_LOG("Graph doesn't contain any nodes!");

        return;
    }

    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    /* Adjacency list maps a node to a set of nodes that must
     * be executed before it. */

    Rr_IndexArray *AdjacencyList =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, Rr_IndexArray, Graph->Nodes.Count);
    Rr_CreateGraphAdjacencyList(Graph, AdjacencyList, Scratch.Arena);

    // Rr_PrintAdjacencyList(Graph->Nodes.Data, AdjacencyList,
    // Graph->Nodes.Count);

    /* Topological sort. */

    int *SortState = RR_ALLOC(Scratch.Arena, sizeof(int) * Graph->Nodes.Count);
    for (size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    {
        Rr_GraphNode *Node = Graph->Nodes.Data[Index];
        if (Node != NULL)
        {
            Rr_SortGraph(
                Node->OriginalIndex,
                AdjacencyList,
                SortState,
                Graph->Nodes.Data,
                SortedNodes);
        }
    }

    /* Split nodes between early and late command buffers. */
    /* TODO: Probably shouldn't require its own pass? */
    /* TODO: Some early nodes still get batched for late execution. */

    for (size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Rr_GraphNode *Node = SortedNodes->Data[Index];
        if (Node->UsesLateCommandBuffer)
        {
            continue;
        }
        Rr_IndexArray *Dependencies = &AdjacencyList[Node->OriginalIndex];
        for (size_t DependencyIndex = 0; DependencyIndex < Dependencies->Count;
             ++DependencyIndex)
        {
            Rr_GraphNode *Dependency =
                Graph->Nodes.Data[Dependencies->Data[DependencyIndex]];
            if (Dependency != NULL)
            {
                Node->UsesLateCommandBuffer |=
                    Dependency->UsesLateCommandBuffer;
            }
        }
    }

    // Rr_PrintAdjacencyList(Graph->Nodes.Data, AdjacencyList,
    // Graph->Nodes.Count);

    /* Longest path search will determine dependency level for each node.
     * Nodes within same dependency level are meant to be batched together. */

    Rr_GraphNode **Reversed =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, Rr_GraphNode *, SortedNodes->Count);
    for (size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Reversed[Index] = SortedNodes->Data[SortedNodes->Count - 1 - Index];
    }

    for (size_t Index = 0; Index < SortedNodes->Count; ++Index)
    {
        Rr_GraphNode *Node = Reversed[Index];
        size_t OriginalIndex = Node->OriginalIndex;
        for (size_t DepIndex = 0; DepIndex < AdjacencyList[OriginalIndex].Count;
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
    Rr_Graph *Graph,
    Rr_GraphNode *Node,
    VkCommandBuffer CommandBuffer)
{
    switch (Node->Type)
    {
        case RR_GRAPH_NODE_TYPE_COMPUTE:
        {
            Rr_ExecuteComputeNode(Graph, &Node->Union.Compute, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_GRAPHICS:
        {
            Rr_ExecuteGraphicsNode(Graph, &Node->Union.Graphics, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE:
        {
            Rr_ExecuteClearColorImageNode(
                Graph,
                &Node->Union.ClearColorImage,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_COPY_BUFFER_TO_IMAGE:
        {
            Rr_ExecuteCopyBufferToImageNode(
                Graph,
                &Node->Union.CopyBufferToImage,
                CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_BLIT:
        {
            Rr_ExecuteBlitNode(Graph, &Node->Union.Blit, CommandBuffer);
        }
        break;
        case RR_GRAPH_NODE_TYPE_TRANSFER:
        {
            Rr_ExecuteTransferNode(Graph, &Node->Union.Transfer, CommandBuffer);
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
    Rr_BarrierBatch *Barrier,
    VkCommandBuffer CommandBuffer,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Device *Device = &gRenderer->Device;

    size_t MaxPossibleBarriers =
        Barrier->BufferBarriers.Count + Barrier->ImageBarriers.Count;

    if (MaxPossibleBarriers == 0)
    {
        return;
    }

    VkPipelineStageFlags SrcStageMaskEarly = 0;
    VkPipelineStageFlags DstStageMaskEarly = 0;
    RR_ARRAY(VkBufferMemoryBarrier) BufferBarriersEarly = { 0 };
    RR_RESERVE_ARRAY(
        &BufferBarriersEarly,
        Barrier->BufferBarriers.Count,
        Scratch.Arena);
    RR_ARRAY(VkImageMemoryBarrier) ImageBarriersEarly = { 0 };
    RR_RESERVE_ARRAY(
        &ImageBarriersEarly,
        Barrier->ImageBarriers.Count,
        Scratch.Arena);

    VkPipelineStageFlags SrcStageMask = 0;
    VkPipelineStageFlags DstStageMask = 0;
    RR_ARRAY(VkBufferMemoryBarrier) BufferBarriers = { 0 };
    RR_RESERVE_ARRAY(
        &BufferBarriers,
        Barrier->BufferBarriers.Count,
        Scratch.Arena);
    RR_ARRAY(VkImageMemoryBarrier) ImageBarriers = { 0 };
    RR_RESERVE_ARRAY(
        &ImageBarriers,
        Barrier->ImageBarriers.Count,
        Scratch.Arena);

    for (size_t Index = 0; Index < Barrier->BufferBarriers.Count; ++Index)
    {
        Rr_BufferMemoryBarrier *BufferBarrier =
            Barrier->BufferBarriers.Data + Index;

        RR_ARRAY(VkBufferMemoryBarrier) * BarriersArray;
        if (BufferBarrier->DstStageMask <= RR_VULKAN_EARLY_STAGES)
        {
            SrcStageMaskEarly |= BufferBarrier->SrcStageMask;
            DstStageMaskEarly |= BufferBarrier->DstStageMask;
            BarriersArray = (void *)&BufferBarriersEarly;
        }
        else
        {
            SrcStageMask |= BufferBarrier->SrcStageMask;
            DstStageMask |= BufferBarrier->DstStageMask;
            BarriersArray = (void *)&BufferBarriers;
        }

        *RR_PUSH_INTO_ARRAY(BarriersArray, Scratch.Arena) =
            (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .buffer = BufferBarrier->Buffer,
                .srcAccessMask = BufferBarrier->SrcAccessMask,
                .dstAccessMask = BufferBarrier->DstAccessMask,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            };

        Rr_SyncState *BufferState =
            Rr_GetSyncState((Rr_MapKey)BufferBarrier->Buffer);
        *BufferState = (Rr_SyncState){
            .StageMask = BufferBarrier->DstStageMask,
            .AccessMask = BufferBarrier->DstAccessMask,
        };
    }

    for (size_t Index = 0; Index < Barrier->ImageBarriers.Count; ++Index)
    {
        Rr_ImageMemoryBarrier *ImageBarrier =
            Barrier->ImageBarriers.Data + Index;

        RR_ARRAY(VkImageMemoryBarrier) * BarriersArray;
        if (ImageBarrier->DstStageMask <= RR_VULKAN_EARLY_STAGES)
        {
            SrcStageMaskEarly |= ImageBarrier->SrcStageMask;
            DstStageMaskEarly |= ImageBarrier->DstStageMask;
            BarriersArray = (void *)&ImageBarriersEarly;
        }
        else
        {
            SrcStageMask |= ImageBarrier->SrcStageMask;
            DstStageMask |= ImageBarrier->DstStageMask;
            BarriersArray = (void *)&ImageBarriers;
        }

        *RR_PUSH_INTO_ARRAY(BarriersArray, Scratch.Arena) =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .image = ImageBarrier->Image,
                .srcAccessMask = ImageBarrier->SrcAccessMask,
                .dstAccessMask = ImageBarrier->DstAccessMask,
                .oldLayout = ImageBarrier->OldLayout,
                .newLayout = ImageBarrier->NewLayout,
                .subresourceRange = ImageBarrier->SubresourceRange,
            };

        Rr_SyncState *ImageState =
            Rr_GetSyncState((Rr_MapKey)ImageBarrier->Image);

        *ImageState = (Rr_SyncState){
            .StageMask = ImageBarrier->DstStageMask,
            .AccessMask = ImageBarrier->DstAccessMask,
            .Layout = ImageBarrier->NewLayout,
        };
    }

    if (BufferBarriersEarly.Count > 0 || ImageBarriersEarly.Count > 0)
    {
        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcStageMaskEarly,
            DstStageMaskEarly,
            0,
            0,
            NULL,
            (uint32_t)BufferBarriersEarly.Count,
            BufferBarriersEarly.Data,
            (uint32_t)ImageBarriersEarly.Count,
            ImageBarriersEarly.Data);
    }

    if (BufferBarriers.Count > 0 || ImageBarriers.Count > 0)
    {
        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcStageMask,
            DstStageMask,
            0,
            0,
            NULL,
            (uint32_t)BufferBarriers.Count,
            BufferBarriers.Data,
            (uint32_t)ImageBarriers.Count,
            ImageBarriers.Data);
    }

    RR_CLEAR_ARRAY(&Barrier->ImageBarriers);
    RR_CLEAR_ARRAY(&Barrier->BufferBarriers);
    Barrier->VulkanHandleToBarrier = NULL;

    Rr_DestroyScratch(Scratch);
}

void Rr_ExecuteGraph(Rr_Graph *Graph, Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_NodeArray SortedNodes = { 0 };
    RR_RESERVE_ARRAY(&SortedNodes, Graph->Nodes.Count, Scratch.Arena);

    Rr_ProcessGraphNodes(Graph, &SortedNodes, Scratch.Arena);

    /* Resolve all referenced resources. */

    for (size_t Index = 0; Index < Graph->Resources.Count; ++Index)
    {
        Rr_GraphResource *Resource = Graph->Resources.Data + Index;
        if (Resource->IsImage)
        {
            Resource->Allocated = Rr_GetCurrentImage(Resource->Container);
        }
        else
        {
            Resource->Allocated =
                Rr_GetCurrentAllocatedBuffer(Resource->Container);
        }
    }

    size_t DependencyLevel = SortedNodes.Data[0]->DependencyLevel;

    Rr_BarrierBatch BarrierBatch = { 0 };

    size_t BatchStartIndex = 0;
    size_t BatchSize = 0;

    bool UseLateCommandBuffer = false;

    for (size_t Index = 0; Index < SortedNodes.Count; ++Index)
    {
        Rr_GraphNode *Node = SortedNodes.Data[Index];
        UseLateCommandBuffer |= Node->UsesLateCommandBuffer;
        DependencyLevel = Node->DependencyLevel;

        for (size_t DepIndex = 0; DepIndex < Node->Dependencies.Count;
             ++DepIndex)
        {
            Rr_NodeDependency *Dependency = Node->Dependencies.Data + DepIndex;
            Rr_SyncState *State = &Dependency->State;

            if (State->Layout != 0)
            {
                /* Image Synchronization */

                Rr_AllocatedImage *AllocatedImage =
                    Rr_GetGraphImage(Graph, Dependency->Handle);
                VkImage Image = AllocatedImage->Handle;

                Rr_SyncState *PrevState = Rr_GetSyncState((Rr_MapKey)Image);

                /* If reading again, just make sure the memory is "available" to
                 * this memory domain AND the image is in the same layout. */

                bool IsReadingNow =
                    RR_HAS_BIT(State->AccessMask, RR_VULKAN_WRITES) == 0;
                bool WasReadingBefore =
                    RR_HAS_BIT(PrevState->AccessMask, RR_VULKAN_WRITES) == 0;
                bool IsSameLayout = State->Layout == PrevState->Layout;
                if (IsReadingNow && WasReadingBefore && IsSameLayout)
                {
                    bool IncludesPreviousAccessMask =
                        (PrevState->AccessMask & State->AccessMask) ==
                        State->AccessMask;
                    if (IncludesPreviousAccessMask)
                    {
                        /* Skip this barrier! */

                        continue;
                    }
                }

                Rr_ImageMemoryBarrier **ImageBarrierRef = RR_GET_MAP_VALUE(
                    &BarrierBatch.VulkanHandleToBarrier,
                    Image,
                    Scratch.Arena);
                Rr_ImageMemoryBarrier *ImageBarrier = *ImageBarrierRef;
                if (ImageBarrier == NULL)
                {
                    *ImageBarrierRef = RR_PUSH_INTO_ARRAY(
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
                        .OldLayout = PrevState->Layout,
                        .NewLayout = State->Layout,
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

                Rr_SyncState *PrevState = Rr_GetSyncState((Rr_MapKey)Buffer);

                /* If reading again, just make sure the memory is "available" to
                 * this memory domain. */

                bool IsReadingNow =
                    RR_HAS_BIT(State->AccessMask, RR_VULKAN_WRITES) == 0;
                bool WasReadingBefore =
                    RR_HAS_BIT(PrevState->AccessMask, RR_VULKAN_WRITES) == 0;
                if (IsReadingNow && WasReadingBefore)
                {
                    bool IncludesPreviousAccessMask =
                        (PrevState->AccessMask & State->AccessMask) ==
                        State->AccessMask;
                    if (IncludesPreviousAccessMask)
                    {
                        /* Skip this barrier! */

                        continue;
                    }
                }

                Rr_BufferMemoryBarrier **BufferBarrierRef = RR_GET_MAP_VALUE(
                    &BarrierBatch.VulkanHandleToBarrier,
                    Buffer,
                    Scratch.Arena);
                Rr_BufferMemoryBarrier *BufferBarrier = *BufferBarrierRef;
                if (BufferBarrier == NULL)
                {
                    *BufferBarrierRef = RR_PUSH_INTO_ARRAY(
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
        if (Index + 1 < SortedNodes.Count)
        {
            LastNodeThisLevel =
                SortedNodes.Data[Index + 1]->DependencyLevel != DependencyLevel;
        }
        else
        {
            LastNode = true;
        }
        if (LastNode || LastNodeThisLevel)
        {
            /* Execute current batch now! */

            VkCommandBuffer CommandBuffer = UseLateCommandBuffer
                                                ? Frame->LateCommandBuffer
                                                : Frame->EarlyCommandBuffer;

            Rr_ApplyBarrierBatch(&BarrierBatch, CommandBuffer, Scratch.Arena);

            for (size_t NodeIndex = BatchStartIndex;
                 NodeIndex < BatchStartIndex + BatchSize;
                 ++NodeIndex)
            {
                Rr_GraphNode *Node = SortedNodes.Data[NodeIndex];
                Rr_ExecuteGraphNode(Graph, Node, CommandBuffer);
            }

            BatchStartIndex = Index + 1;
            BatchSize = 0;
            UseLateCommandBuffer = 0;
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

    Rr_GraphHandle **GraphHandle = RR_GET_MAP_VALUE(
        &Graph->Handles,
        (Rr_MapKey)Container,
        Graph->Frame->Arena);
    if (*GraphHandle == NULL)
    {
        Rr_GraphImage Handle = {
            .Values.Index = (uint32_t)Graph->Resources.Count,
        };
        *RR_PUSH_INTO_ARRAY(&Graph->Resources, Graph->Frame->Arena) =
            (Rr_GraphResource){
                .Container = Container,
                .IsImage = IsImage,
            };
        *GraphHandle = RR_ALLOC_TYPE(Graph->Frame->Arena, Rr_GraphHandle);
        **GraphHandle = Handle;

        if (IsImage)
        {
            *RR_PUSH_INTO_ARRAY(
                &Graph->Frame->UsedImages,
                Graph->Frame->Arena) = Container;
        }
        else
        {
            *RR_PUSH_INTO_ARRAY(
                &Graph->Frame->UsedBuffers,
                Graph->Frame->Arena) = Container;
        }
    }

    return *GraphHandle;
}

Rr_GraphBuffer *Rr_GetGraphBufferHandle(Rr_Graph *Graph, void *Container)
{
    return Rr_GetGraphHandle(Graph, Container, false);
}

Rr_GraphImage *Rr_GetGraphImageHandle(Rr_Graph *Graph, void *Container)
{
    return Rr_GetGraphHandle(Graph, Container, true);
}

Rr_GraphNode *Rr_AddTransferNode(Rr_Graph *Graph, const char *Name)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_TRANSFER, Name);

    Rr_TransferNode *TransferNode = &GraphNode->Union.Transfer;
    RR_RESERVE_ARRAY(&TransferNode->Transfers, 2, Frame->Arena);

    return GraphNode;
}

void Rr_TransferBufferData(
    Rr_GraphNode *Node,
    size_t Size,
    Rr_Buffer *SrcBuffer,
    size_t SrcOffset,
    Rr_Buffer *DstBuffer,
    size_t DstOffset)
{
    Rr_TransferNode *TransferNode = &Node->Union.Transfer;

    Rr_GraphBuffer *SrcBufferHandle =
        Rr_GetGraphBufferHandle(Node->Graph, SrcBuffer);
    Rr_GraphBuffer *DstBufferHandle =
        Rr_GetGraphBufferHandle(Node->Graph, DstBuffer);

    *RR_PUSH_INTO_ARRAY(&TransferNode->Transfers, Node->Graph->Frame->Arena) =
        (Rr_Transfer){
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
    Rr_Graph *Graph,
    const char *Name,
    Rr_Image2D *SrcImage,
    Rr_Image2D *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_ImageAspect ImageAspect)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_BLIT, Name);

    Rr_GraphImage *SrcImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, SrcImage);
    Rr_GraphImage *DstImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, DstImage);

    Rr_BlitNode *BlitNode = &GraphNode->Union.Blit;
    *BlitNode = (Rr_BlitNode){
        .SrcImageHandle = *SrcImageHandle,
        .DstImageHandle = *DstImageHandle,
        .SrcRect = SrcRect,
        .DstRect = DstRect,
    };

    BlitNode->AspectMask = Rr_ToVulkanImageAspect(ImageAspect);

    Rr_AddNodeDependency(
        GraphNode,
        SrcImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        });

    Rr_AddNodeDependency(
        GraphNode,
        DstImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });

    return GraphNode;
}

Rr_GraphNode *Rr_AddComputeNode(Rr_Graph *Graph, const char *Name)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_COMPUTE, Name);

    Rr_ComputeNode *ComputeNode = &GraphNode->Union.Compute;

    ComputeNode->Encoded.Encoded =
        RR_ALLOC(Frame->Arena, sizeof(Rr_NodeFunction));
    ComputeNode->Encoded.EncodedFirst = ComputeNode->Encoded.Encoded;

    return GraphNode;
}

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_Graph *Graph,
    const char *Name,
    size_t ColorTargetCount,
    Rr_ColorTarget *ColorTargets,
    Rr_Image2D **ColorImages,
    Rr_DepthTarget *DepthTarget,
    Rr_Image2D *DepthImage)
{
    assert(ColorTargetCount > 0 || DepthTarget != NULL);
    assert(DepthTarget == NULL || DepthImage != NULL);

    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_GRAPHICS, Name);

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.Graphics;
    if (ColorTargetCount > 0)
    {
        GraphicsNode->ColorTargetCount = ColorTargetCount;
        GraphicsNode->ColorTargets =
            RR_ALLOC_TYPE_COUNT(Frame->Arena, Rr_ColorTarget, ColorTargetCount);
        GraphicsNode->ColorImages =
            RR_ALLOC_TYPE_COUNT(Frame->Arena, Rr_GraphImage, ColorTargetCount);

        for (size_t Index = 0; Index < ColorTargetCount; ++Index)
        {
            assert(ColorImages[Index] != NULL);

            Rr_GraphImage *ColorImageHandle =
                Rr_GetGraphImageHandle(Frame->Graph, ColorImages[Index]);

            GraphicsNode->ColorTargets[Index] = ColorTargets[Index];
            GraphicsNode->ColorImages[Index] = *ColorImageHandle;

            VkAccessFlags AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            if (ColorTargets[Index].LoadOp == RR_LOAD_OP_LOAD)
            {
                AccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            }
            Rr_AddNodeDependency(
                GraphNode,
                ColorImageHandle,
                &(Rr_SyncState){
                    .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .AccessMask = AccessMask,
                    .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });
        }
    }
    if (DepthTarget != NULL)
    {
        Rr_GraphImage *DepthImageHandle = Rr_GetGraphImageHandle(
            Frame->Graph,
            (Rr_ImageContainer *)DepthImage);

        GraphicsNode->DepthTarget = RR_ALLOC_TYPE(Frame->Arena, Rr_DepthTarget);
        *GraphicsNode->DepthTarget = *DepthTarget;
        GraphicsNode->DepthImage = *DepthImageHandle;

        VkAccessFlags AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (DepthTarget->LoadOp == RR_LOAD_OP_LOAD)
        {
            AccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        Rr_AddNodeDependency(
            GraphNode,
            DepthImageHandle,
            &(Rr_SyncState){
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .AccessMask = AccessMask,
                .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            });
    }

    GraphicsNode->Encoded.Encoded =
        RR_ALLOC(Frame->Arena, sizeof(Rr_NodeFunction));
    GraphicsNode->Encoded.EncodedFirst = GraphicsNode->Encoded.Encoded;

    return GraphNode;
}

Rr_GraphNode *Rr_AddClearColorImageNode(
    Rr_Graph *Graph,
    const char *Name,
    Rr_ColorClear *ColorClear,
    Rr_Image2D *Image)
{
    assert(ColorClear != NULL && Image != NULL);

    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE, Name);

    Rr_GraphImage *ColorImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, Image);

    Rr_AddNodeDependency(
        GraphNode,
        ColorImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        });

    GraphNode->Union.ClearColorImage =
        (Rr_ClearColorImageNode){ .ColorClear = *ColorClear,
                                  .ColorImage = *ColorImageHandle };

    return GraphNode;
}

static inline Rr_GraphNode *Rr_AddCopyBufferToImageNode(
    Rr_Graph *Graph,
    const char *Name,
    Rr_Buffer *Buffer,
    size_t BufferOffset,
    Rr_ImageContainer *Image,
    uint32_t BaseLayer,
    uint32_t LayerCount,
    uint32_t MipLevel)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Rr_GraphNode *GraphNode =
        Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_COPY_BUFFER_TO_IMAGE, Name);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Frame->Graph, Image);

    Rr_GraphBuffer *BufferHandle =
        Rr_GetGraphBufferHandle(Frame->Graph, Buffer);

    Rr_AddNodeDependency(
        GraphNode,
        ImageHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });

    Rr_AddNodeDependency(
        GraphNode,
        BufferHandle,
        &(Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        });

    GraphNode->Union.CopyBufferToImage = (Rr_CopyBufferToImageNode){
        .Buffer = *BufferHandle,
        .BufferOffset = BufferOffset,
        .Image = *ImageHandle,
        .BaseLayer = BaseLayer,
        .LayerCount = LayerCount,
        .MipLevel = MipLevel,
    };

    return GraphNode;
}

Rr_GraphNode *Rr_AddCopyBufferToImage2DNode(
    Rr_Graph *Graph,
    const char *Name,
    Rr_Buffer *Buffer,
    size_t BufferOffset,
    Rr_Image2D *Image,
    uint32_t MipLevel)
{
    assert(Graph != NULL && Buffer != NULL && Image != NULL);

    return Rr_AddCopyBufferToImageNode(
        Graph,
        Name,
        Buffer,
        BufferOffset,
        (Rr_ImageContainer *)Image,
        0,
        1,
        MipLevel);
}

Rr_GraphNode *Rr_AddCopyBufferToImageCubeNode(
    Rr_Graph *Graph,
    const char *Name,
    Rr_Buffer *Buffer,
    size_t BufferOffset,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace Face,
    uint32_t MipLevel)
{
    assert(Graph != NULL);
    assert(Buffer != NULL);
    assert(ImageCube != NULL);

    return Rr_AddCopyBufferToImageNode(
        Graph,
        Name,
        Buffer,
        BufferOffset,
        (Rr_ImageContainer *)ImageCube,
        (uint32_t)Face,
        1,
        MipLevel);
}

Rr_GraphNode *Rr_AddCopyBufferToImageCubeNodeEx(
    Rr_Graph *Graph,
    const char *Name,
    Rr_Buffer *Buffer,
    size_t BufferOffset,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace FirstFace,
    Rr_ImageCubeFace LastFace,
    uint32_t MipLevel)
{
    assert(Graph != NULL);
    assert(Buffer != NULL);
    assert(ImageCube != NULL);
    assert(FirstFace <= LastFace);
    assert(FirstFace >= RR_IMAGE_CUBE_FACE_FIRST);
    assert(LastFace <= RR_IMAGE_CUBE_FACE_LAST);

    return Rr_AddCopyBufferToImageNode(
        Graph,
        Name,
        Buffer,
        BufferOffset,
        (Rr_ImageContainer *)ImageCube,
        (uint32_t)FirstFace,
        1 + ((uint32_t)LastFace - (uint32_t)FirstFace),
        MipLevel);
}

#define RR_NODE_ENCODE(FunctionType, ArgsType)                         \
    Rr_Arena *Arena = Node->Graph->Frame->Arena;                       \
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
    size_t VertexCount,
    size_t InstanceCount,
    size_t FirstVertex,
    size_t FirstInstance)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DRAW, Rr_DrawArgs) = (Rr_DrawArgs){
        .VertexCount = (uint32_t)VertexCount,
        .InstanceCount = (uint32_t)InstanceCount,
        .FirstVertex = (uint32_t)FirstVertex,
        .FirstInstance = (uint32_t)FirstInstance,
    };
}

void Rr_DrawIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Offset,
    size_t Count,
    size_t Stride)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DRAW_INDIRECT, Rr_DrawIndirectArgs) =
        (Rr_DrawIndirectArgs){
            .BufferHandle = *BufferHandle,
            .Offset = (uint32_t)Offset,
            .Count = (uint32_t)Count,
            .Stride = (uint32_t)Stride,
        };

    Rr_AddNodeDependency(
        Node,
        BufferHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        });
}

void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    size_t IndexCount,
    size_t InstanceCount,
    size_t FirstIndex,
    int32_t VertexOffset,
    size_t FirstInstance)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_DRAW_INDEXED, Rr_DrawIndexedArgs) =
        (Rr_DrawIndexedArgs){
            .IndexCount = (uint32_t)IndexCount,
            .InstanceCount = (uint32_t)InstanceCount,
            .FirstIndex = (uint32_t)FirstIndex,
            .VertexOffset = VertexOffset,
            .FirstInstance = (uint32_t)FirstInstance,
        };
}

void Rr_BindVertexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Slot,
    size_t Offset)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
        Rr_BindIndexBufferArgs) = (Rr_BindIndexBufferArgs){
        .BufferHandle = *BufferHandle,
        .Slot = (uint32_t)Slot,
        .Offset = (uint32_t)Offset,
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
    size_t Slot,
    size_t Offset,
    Rr_IndexType Type)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
        Rr_BindIndexBufferArgs) = (Rr_BindIndexBufferArgs){
        .BufferHandle = *BufferHandle,
        .Slot = (uint32_t)Slot,
        .Offset = (uint32_t)Offset,
        .Type = Rr_ToVulkanIndexType(Type),
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
    assert(
        Node->Union.Graphics.ColorTargetCount ==
        GraphicsPipeline->ColorAttachmentCount);
    assert(
        Node->Union.Graphics.DepthTarget != NULL ||
        !GraphicsPipeline->HasDepthStencil);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
        Rr_GraphicsPipeline *) = GraphicsPipeline;
}

void Rr_SetViewport(Rr_GraphNode *Node, Rr_Rect *Rect)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_SET_VIEWPORT, Rr_Rect) = *Rect;
}

void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntRect *Rect)
{
    assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_SET_SCISSOR, Rr_IntRect) = *Rect;
}

void Rr_BindSampler(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    size_t Set,
    size_t Binding)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Sampler != NULL);

    RR_NODE_ENCODE(RR_NODE_FUNCTION_TYPE_BIND_SAMPLER, Rr_BindSamplerArgs) =
        (Rr_BindSamplerArgs){
            .Sampler = Sampler,
            .Set = (uint32_t)Set,
            .Binding = (uint32_t)Binding,
        };
}

void Rr_BindSampledImage(
    Rr_GraphNode *Node,
    Rr_Image2D *Image,
    size_t Set,
    size_t Binding)
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
        .Set = (uint32_t)Set,
        .Binding = (uint32_t)Binding,
    };

    /* TODO: Stage mask can be infered from pipeline layout. */

    Rr_AddNodeDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .Layout = Layout,
        });
}

void Rr_BindCombinedImageSampler(
    Rr_GraphNode *Node,
    Rr_Image2D *Image,
    Rr_Sampler *Sampler,
    size_t Set,
    size_t Binding)
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
        .Set = (uint32_t)Set,
        .Binding = (uint32_t)Binding,
    };

    /* TODO: Stage mask can be infered from pipeline layout. */

    Rr_AddNodeDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .Layout = Layout,
        });
}

void Rr_BindCombinedCubemapSampler(
    Rr_GraphNode *Node,
    Rr_ImageCube *Cubemap,
    Rr_Sampler *Sampler,
    size_t Set,
    size_t Binding)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Sampler != NULL);
    assert(Cubemap);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Cubemap);

    VkImageLayout Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER,
        Rr_BindCombinedImageSamplerArgs) = (Rr_BindCombinedImageSamplerArgs){
        .ImageHandle = *ImageHandle,
        .Layout = Layout,
        .Sampler = Sampler,
        .Set = (uint32_t)Set,
        .Binding = (uint32_t)Binding,
    };

    /* TODO: Stage mask can be infered from pipeline layout. */

    Rr_AddNodeDependency(
        Node,
        ImageHandle,
        &(Rr_SyncState){
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .Layout = Layout,
        });
}

void Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Set,
    size_t Binding,
    size_t Offset,
    size_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER,
        Rr_BindUniformBufferArgs) = (Rr_BindUniformBufferArgs){
        .BufferHandle = *BufferHandle,
        .Set = (uint32_t)Set,
        .Binding = (uint32_t)Binding,
        .Offset = (uint32_t)Offset,
        .Size = (uint32_t)Size,
    };

    /* TODO: Proper stage can be infered from pipeline layout. */

    if (Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE)
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
    size_t Set,
    size_t Binding,
    size_t Offset,
    size_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);

    Rr_GraphBuffer *BufferHandle = Rr_GetGraphBufferHandle(Node->Graph, Buffer);

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER,
        Rr_BindStorageBufferArgs) = (Rr_BindStorageBufferArgs){
        .BufferHandle = *BufferHandle,
        .Set = (uint32_t)Set,
        .Binding = (uint32_t)Binding,
        .Offset = (uint32_t)Offset,
        .Size = (uint32_t)Size,
    };

    /* TODO: Proper read/write stuff. */

    if (Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE)
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

void Rr_BindStorageImage(
    Rr_GraphNode *Node,
    Rr_Image2D *Image,
    size_t Set,
    size_t Binding)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);

    Rr_GraphImage *ImageHandle = Rr_GetGraphImageHandle(Node->Graph, Image);

    VkImageLayout Layout = VK_IMAGE_LAYOUT_GENERAL;

    RR_NODE_ENCODE(
        RR_NODE_FUNCTION_TYPE_BIND_STORAGE_IMAGE,
        Rr_BindStorageImageArgs) = (Rr_BindStorageImageArgs){
        .ImageHandle = *ImageHandle,
        .Set = (uint32_t)Set,
        .Binding = (uint32_t)Binding,
    };

    /* TODO: Proper read/write stuff. */

    if (Node->Type == RR_GRAPH_NODE_TYPE_COMPUTE)
    {
        Rr_AddNodeDependency(
            Node,
            ImageHandle,
            &(Rr_SyncState){
                .AccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .StageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .Layout = Layout,
            });
    }
    else
    {
        Rr_AddNodeDependency(
            Node,
            ImageHandle,
            &(Rr_SyncState){
                .AccessMask = VK_ACCESS_SHADER_READ_BIT,
                .StageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .Layout = Layout,
            });
    }
}
