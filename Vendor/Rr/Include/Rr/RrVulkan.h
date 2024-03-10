#pragma once

#define VK_NO_PROTOTYPES

#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_assert.h>

#define VK_ASSERT(Expr)                                                                                                                  \
    {                                                                                                                                    \
        VkResult ExprResult = Expr;                                                                                                      \
        if (ExprResult != VK_SUCCESS)                                                                                                    \
        {                                                                                                                                \
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Assertion " #Expr " == VK_SUCCESS failed! Result is %s", string_VkResult(ExprResult)); \
            SDL_assert(0);                                                                                                               \
        }                                                                                                                                \
    }
