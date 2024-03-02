#pragma once

#define VK_NO_PROTOTYPES

#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK_ASSERT(Expr)                                                                                                              \
    {                                                                                                                                \
        VkResult Result = Expr;                                                                                                      \
        if (Result != VK_SUCCESS)                                                                                                    \
        {                                                                                                                            \
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Assertion " #Expr " == VK_SUCCESS failed! Result is %s", string_VkResult(Result)); \
            SDL_assert(0);                                                                                                           \
        }                                                                                                                            \
    }
