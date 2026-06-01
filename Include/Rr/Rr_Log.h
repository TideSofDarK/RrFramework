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

#ifndef RR_LOG_H
#define RR_LOG_H

#include <Rr/Rr_Defines.h>

#ifdef __cplusplis
#include <cstdarg>
#else
#include <stdarg.h>
#endif

typedef enum
{
    RR_LOG_CATEGORY_APP,
    RR_LOG_CATEGORY_PLATFORM,
    RR_LOG_CATEGORY_SPIRV,
    RR_LOG_CATEGORY_VULKAN,
    RR_LOG_CATEGORY_RENDERER,
    RR_LOG_CATEGORY_GRAPH,
    RR_LOG_CATEGORY_DESCRIPTOR,
    RR_LOG_CATEGORY_UI,
    RR_LOG_CATEGORY_ARENA,
    RR_LOG_CATEGORY_CUSTOM, /* Extend categories starting with this one. */
} Rr_LogCategories;
typedef uint32_t Rr_LogCategory;

typedef enum
{
    RR_LOG_PRIORITY_NONE,
    RR_LOG_PRIORITY_ERROR,
    RR_LOG_PRIORITY_WARNING,
    RR_LOG_PRIORITY_INFO,
    RR_LOG_PRIORITY_TRACE,
} Rr_LogPriorities;
typedef uint32_t Rr_LogPriority;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Rr_LogFunc)(
    uint32_t Category,
    Rr_LogPriority Priority,
    char const *Format,
    va_list Args);

extern char const *RR_CC Rr_LogCategoryString(Rr_LogCategory Category);

extern void RR_CC Rr_SetLogPriority(Rr_LogPriority Priority);

extern void RR_CC Rr_SetLogFunction(Rr_LogFunc Function);

extern Rr_LogFunc RR_CC Rr_GetDefaultLogFunction(void);

extern void RR_CC
Rr_Log(uint32_t Category, Rr_LogPriority Priority, char const *Format, ...);

extern void RR_CC Rr_LogError(uint32_t Category, char const *Format, ...);

extern void RR_CC Rr_LogWarning(uint32_t Category, char const *Format, ...);

extern void RR_CC Rr_LogInfo(uint32_t Category, char const *Format, ...);

extern void RR_CC Rr_LogTrace(uint32_t Category, char const *Format, ...);

#ifdef __cplusplus
}
#endif

#endif
