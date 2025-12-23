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

#include <Rr/Rr_Log.h>

#include <stdio.h>

/* TODO: Maybe make current function and priority thread-local? */

char const *Rr_LogCategoryString(Rr_LogCategory Category)
{
    switch (Category)
    {
        case RR_LOG_CATEGORY_APP:
            return "APPL";
        case RR_LOG_CATEGORY_VULKAN:
            return "VULK";
        case RR_LOG_CATEGORY_RENDERER:
            return "REND";
        case RR_LOG_CATEGORY_IMAGE:
            return "IMAG";
        case RR_LOG_CATEGORY_BUFFER:
            return "BUFF";
        case RR_LOG_CATEGORY_GRAPH:
            return "GRPH";
        case RR_LOG_CATEGORY_DESCRIPTOR:
            return "DESC";
        case RR_LOG_CATEGORY_PLATFORM:
            return "PLAT";
        case RR_LOG_CATEGORY_UI:
            return "IMGU";
        case RR_LOG_CATEGORY_GLTF:
            return "GLTF";
        case RR_LOG_CATEGORY_ARENA:
            return "AREN";
        default:
            return "CUST";
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
