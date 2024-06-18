#include "Rr_Log.h"

#include <SDL3/SDL_log.h>

#include <stdlib.h>

enum
{
    RR_LOG_CATEGORY_GENERAL = SDL_LOG_CATEGORY_CUSTOM,
    RR_LOG_CATEGORY_VULKAN,
    RR_LOG_CATEGORY_MEMORY,
};

void Rr_LogAbort(Rr_CString Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_GENERAL, SDL_LOG_PRIORITY_CRITICAL, Format, Args);
    va_end(Args);
    abort();
}

void Rr_LogVulkan(Rr_CString Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_VULKAN, SDL_LOG_PRIORITY_INFO, Format, Args);
    va_end(Args);
}

void Rr_LogMemory(Rr_CString Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    SDL_LogMessageV(RR_LOG_CATEGORY_MEMORY, SDL_LOG_PRIORITY_INFO, Format, Args);
    va_end(Args);
}
