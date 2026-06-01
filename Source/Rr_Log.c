/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <Rr/Rr_Log.h>

#include <stdio.h>

/* TODO: Maybe make current function and priority thread-local? */

char const *Rr_LogCategoryString(Rr_LogCategory Category)
{
    switch (Category)
    {
        case RR_LOG_CATEGORY_APP:
            return "Application";
        case RR_LOG_CATEGORY_SPIRV:
            return "SPIR-V";
        case RR_LOG_CATEGORY_VULKAN:
            return "Vulkan";
        case RR_LOG_CATEGORY_RENDERER:
            return "Renderer";
        case RR_LOG_CATEGORY_GRAPH:
            return "Graph";
        case RR_LOG_CATEGORY_DESCRIPTOR:
            return "Descriptor";
        case RR_LOG_CATEGORY_PLATFORM:
            return "Platform";
        case RR_LOG_CATEGORY_UI:
            return "UI";
        case RR_LOG_CATEGORY_ARENA:
            return "Arena";
        default:
            return "Custom";
    }
}

static uint32_t gLogPriority = RR_LOG_PRIORITY_TRACE;

void Rr_SetLogPriority(Rr_LogPriority Priority)
{
    gLogPriority = Priority;
}

static void Rr_DefaultLogFunction(
    uint32_t Category,
    Rr_LogPriority Priority,
    char const *Format,
    va_list Args)
{
    FILE *Out = stdout;
    if (Priority == RR_LOG_PRIORITY_ERROR)
    {
        Out = stderr;
    }
    /* TODO: Temporary. */
#ifdef _WIN32
    Out = stderr;
#endif
    fprintf(Out, "[%s] ", Rr_LogCategoryString(Category));
    vfprintf(Out, Format, Args);
    fprintf(Out, "\n");
}

static Rr_LogFunc gLogFunction = Rr_DefaultLogFunction;

void Rr_SetLogFunction(Rr_LogFunc Function)
{
    gLogFunction = Function;
}

Rr_LogFunc Rr_GetDefaultLogFunction(void)
{
    return Rr_DefaultLogFunction;
}

#define RR_CHECK_PRIORITY(Priority) \
    if (gLogPriority < (Priority))  \
    {                               \
        return;                     \
    }

void Rr_Log(uint32_t Category, Rr_LogPriority Priority, char const *Format, ...)
{
    RR_CHECK_PRIORITY(Priority);
    va_list Args;
    va_start(Args, Format);
    gLogFunction(Category, Priority, Format, Args);
    va_end(Args);
}

void Rr_LogError(uint32_t Category, char const *Format, ...)
{
    RR_CHECK_PRIORITY(RR_LOG_PRIORITY_ERROR);
    va_list Args;
    va_start(Args, Format);
    gLogFunction(Category, RR_LOG_PRIORITY_ERROR, Format, Args);
    va_end(Args);
}

void Rr_LogWarning(uint32_t Category, char const *Format, ...)
{
    RR_CHECK_PRIORITY(RR_LOG_PRIORITY_WARNING);
    va_list Args;
    va_start(Args, Format);
    gLogFunction(Category, RR_LOG_PRIORITY_WARNING, Format, Args);
    va_end(Args);
}

void Rr_LogInfo(uint32_t Category, char const *Format, ...)
{
    RR_CHECK_PRIORITY(RR_LOG_PRIORITY_INFO);
    va_list Args;
    va_start(Args, Format);
    gLogFunction(Category, RR_LOG_PRIORITY_INFO, Format, Args);
    va_end(Args);
}

void Rr_LogTrace(uint32_t Category, char const *Format, ...)
{
    RR_CHECK_PRIORITY(RR_LOG_PRIORITY_INFO);
    va_list Args;
    va_start(Args, Format);
    gLogFunction(Category, RR_LOG_PRIORITY_TRACE, Format, Args);
    va_end(Args);
}
