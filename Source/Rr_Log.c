#include "Rr_Log.h"

#include "Rr/Rr_Defines.h"

#include <SDL3/SDL_log.h>

#include <stdlib.h>

enum
{
    RR_LOG_CATEGORY_GENERAL = SDL_LOG_CATEGORY_CUSTOM,
    RR_LOG_CATEGORY_VULKAN,
    RR_LOG_CATEGORY_MEMORY,
    RR_LOG_CATEGORY_RENDER,
};

void Rr_LogAbort(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_GENERAL, SDL_LOG_PRIORITY_CRITICAL, Format, Args);
    va_end(Args);
    abort();
}

void Rr_LogRender(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_INFO, Format, Args);
    va_end(Args);
}

void Rr_LogVulkan(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_VULKAN, SDL_LOG_PRIORITY_INFO, Format, Args);
    va_end(Args);
}

void Rr_LogMemory(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_MEMORY, SDL_LOG_PRIORITY_INFO, Format, Args);
    va_end(Args);
}
