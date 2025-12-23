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
    RR_LOG_CATEGORY_VULKAN,
    RR_LOG_CATEGORY_RENDERER,
    RR_LOG_CATEGORY_IMAGE,
    RR_LOG_CATEGORY_BUFFER,
    RR_LOG_CATEGORY_GRAPH,
    RR_LOG_CATEGORY_DESCRIPTOR,
    RR_LOG_CATEGORY_PLATFORM,
    RR_LOG_CATEGORY_UI,
    RR_LOG_CATEGORY_GLTF,
    RR_LOG_CATEGORY_ARENA,
    RR_LOG_CATEGORY_CUSTOM, /* Extend categories starting with this one. */
} Rr_LogCategory;

typedef enum
{
    RR_LOG_PRIORITY_ERROR,
    RR_LOG_PRIORITY_WARNING,
    RR_LOG_PRIORITY_INFO,
    RR_LOG_PRIORITY_TRACE,
} Rr_LogPriority;

typedef void (*Rr_LogFunc)(
    uint32_t Category,
    Rr_LogPriority Priority,
    char const *Format,
    va_list Args);

RR_EXTERN void Rr_SetLogFunction(Rr_LogFunc Function);

RR_EXTERN Rr_LogFunc Rr_GetDefaultLogFunction(void);

RR_EXTERN void Rr_Log(
    uint32_t Category,
    Rr_LogPriority Priority,
    char const *Format,
    ...);

RR_EXTERN void Rr_LogError(uint32_t Category, char const *Format, ...);

RR_EXTERN void Rr_LogWarning(uint32_t Category, char const *Format, ...);

RR_EXTERN void Rr_LogInfo(uint32_t Category, char const *Format, ...);

RR_EXTERN void Rr_LogTrace(uint32_t Category, char const *Format, ...);

#endif
