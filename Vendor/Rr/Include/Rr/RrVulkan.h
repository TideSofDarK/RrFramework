#pragma once

#define VK_NO_PROTOTYPES

#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#define RR_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define RR_COLOR_FORMAT VK_FORMAT_R8G8B8A8_UNORM

#define VK_ASSERT(Expr)                                                                                                                  \
    {                                                                                                                                    \
        VkResult ExprResult = Expr;                                                                                                      \
        if (ExprResult != VK_SUCCESS)                                                                                                    \
        {                                                                                                                                \
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Assertion " #Expr " == VK_SUCCESS failed! Result is %s", string_VkResult(ExprResult)); \
            SDL_assert(0);                                                                                                               \
        }                                                                                                                                \
    }
