#include "Rr_Barrier.h"

#include "Rr_Vulkan.h"

void Rr_ChainImageBarrier_Aspect(
    Rr_ImageBarrier* TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout,
    VkImageAspectFlagBits Aspect)
{
    vkCmdPipelineBarrier(
        TransitionImage->CommandBuffer,
        TransitionImage->StageMask,
        DstStageMask,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .image = TransitionImage->Image,
            .oldLayout = TransitionImage->Layout,
            .newLayout = NewLayout,
            .subresourceRange = GetImageSubresourceRange(Aspect),
            .srcAccessMask = TransitionImage->AccessMask,
            .dstAccessMask = DstAccessMask,
        });

    TransitionImage->Layout = NewLayout;
    TransitionImage->StageMask = DstStageMask;
    TransitionImage->AccessMask = DstAccessMask;
}

void Rr_ChainImageBarrier(
    Rr_ImageBarrier* TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout)
{
    Rr_ChainImageBarrier_Aspect(
        TransitionImage,
        DstStageMask,
        DstAccessMask,
        NewLayout,
        (NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
}
