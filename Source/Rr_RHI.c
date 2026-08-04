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

#include "Rr_RHI.h"

#include "Rr_Thread.h"

#include <stdio.h>
#include <string.h>

static RR_THREAD_LOCAL char NextObjectName[RR_MAX_OBJECT_NAME_LENGTH] = { 0 };

Rr_RHI *gRHI;

bool Rr_IsDepthStencilFormat(Rr_ImageFormat Format)
{
    return Format == RR_IMAGE_FORMAT_D24_UNORM_S8_UINT ||
           Format == RR_IMAGE_FORMAT_D32_SFLOAT_S8_UINT;
}

bool Rr_IsDepthFormat(Rr_ImageFormat Format)
{
    return Format == RR_IMAGE_FORMAT_D16_UNORM ||
           Format == RR_IMAGE_FORMAT_D32_SFLOAT;
}

bool Rr_IsSRGBFormat(Rr_ImageFormat Format)
{
    return Format == RR_IMAGE_FORMAT_R8_SRGB ||
           Format == RR_IMAGE_FORMAT_R8G8_SRGB ||
           Format == RR_IMAGE_FORMAT_R8G8B8A8_SRGB ||
           Format == RR_IMAGE_FORMAT_B8G8R8A8_SRGB;
}

char const *RR_CC Rr_GetImageFormatString(Rr_ImageFormat Format)
{
    return Rr_GetImageFormatStrings()[Format];
}

char const *const *RR_CC Rr_GetImageFormatStrings(void)
{
    static char const *IMAGE_FORMATS[] = {
        /* */
        "RR_IMAGE_FORMAT_UNDEFINED",
        /* R8 */
        "RR_IMAGE_FORMAT_R8_UNORM",
        "RR_IMAGE_FORMAT_R8_UNORM",
        "RR_IMAGE_FORMAT_R8_UINT",
        "RR_IMAGE_FORMAT_R8_SINT",
        "RR_IMAGE_FORMAT_R8_SRGB",
        /* R8G8 */
        "RR_IMAGE_FORMAT_R8G8_UNORM",
        "RR_IMAGE_FORMAT_R8G8_SNORM",
        "RR_IMAGE_FORMAT_R8G8_UINT",
        "RR_IMAGE_FORMAT_R8G8_SINT",
        "RR_IMAGE_FORMAT_R8G8_SRGB",
        /* R8G8B8A8 */
        "RR_IMAGE_FORMAT_R8G8B8A8_UNORM",
        "RR_IMAGE_FORMAT_R8G8B8A8_SNORM",
        "RR_IMAGE_FORMAT_R8G8B8A8_SINT",
        "RR_IMAGE_FORMAT_R8G8B8A8_UINT",
        "RR_IMAGE_FORMAT_R8G8B8A8_SRGB",
        /* B8G8R8A8 */
        "RR_IMAGE_FORMAT_B8G8R8A8_UNORM",
        "RR_IMAGE_FORMAT_B8G8R8A8_SNORM",
        "RR_IMAGE_FORMAT_B8G8R8A8_UINT",
        "RR_IMAGE_FORMAT_B8G8R8A8_SINT",
        "RR_IMAGE_FORMAT_B8G8R8A8_SRGB",
        /* R16 */
        "RR_IMAGE_FORMAT_R16_UNORM",
        "RR_IMAGE_FORMAT_R16_SNORM",
        "RR_IMAGE_FORMAT_R16_UINT",
        "RR_IMAGE_FORMAT_R16_SINT",
        "RR_IMAGE_FORMAT_R16_SFLOAT",
        /* R16G16 */
        "RR_IMAGE_FORMAT_R16G16_UNORM",
        "RR_IMAGE_FORMAT_R16G16_SNORM",
        "RR_IMAGE_FORMAT_R16G16_UINT",
        "RR_IMAGE_FORMAT_R16G16_SINT",
        "RR_IMAGE_FORMAT_R16G16_SFLOAT",
        /* R16G16B16A16 */
        "RR_IMAGE_FORMAT_R16G16B16A16_UNORM",
        "RR_IMAGE_FORMAT_R16G16B16A16_SNORM",
        "RR_IMAGE_FORMAT_R16G16B16A16_SINT",
        "RR_IMAGE_FORMAT_R16G16B16A16_UINT",
        "RR_IMAGE_FORMAT_R16G16B16A16_SFLOAT",
        /* R32 */
        "RR_IMAGE_FORMAT_R32_UINT",
        "RR_IMAGE_FORMAT_R32_SINT",
        "RR_IMAGE_FORMAT_R32_SFLOAT",
        /* R32G32 */
        "RR_IMAGE_FORMAT_R32G32_UINT",
        "RR_IMAGE_FORMAT_R32G32_SINT",
        "RR_IMAGE_FORMAT_R32G32_SFLOAT",
        /* R32G32B32A32 */
        "RR_IMAGE_FORMAT_R32G32B32A32_SINT",
        "RR_IMAGE_FORMAT_R32G32B32A32_UINT",
        "RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT",
        /* */
        "RR_IMAGE_FORMAT_D16_UNORM",
        "RR_IMAGE_FORMAT_D32_SFLOAT",
        "RR_IMAGE_FORMAT_D24_UNORM_S8_UINT",
        "RR_IMAGE_FORMAT_D32_SFLOAT_S8_UINT",
    };

    return IMAGE_FORMATS;
}

char const *Rr_GetPresentModeString(Rr_PresentMode PresentMode)
{
    return Rr_GetPresentModeStrings()[(size_t)PresentMode];
}

char const *const *RR_CC Rr_GetPresentModeStrings(void)
{
    static char const *PRESENT_MODES[] = {
        "FIFO",
        "FIFO_RELAXED",
        "IMMEDIATE",
        "MAILBOX",
    };

    return PRESENT_MODES;
}

void Rr_SetNextObjectName(const char *Name)
{
    if (!Name)
    {
        return;
    }

    size_t Length = strlen(Name);
    if (!Length)
    {
        return;
    }

    if (Length > RR_MAX_OBJECT_NAME_LENGTH - 1)
    {
        memcpy(NextObjectName, Name, RR_MAX_OBJECT_NAME_LENGTH - 1);
        NextObjectName[RR_MAX_OBJECT_NAME_LENGTH - 1] = '\0';

        return;
    }

    memcpy(NextObjectName, Name, Length);
    NextObjectName[Length] = '\0';
}

void Rr_SetNextObjectNameF(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(NextObjectName, sizeof(NextObjectName), Format, Args);
    va_end(Args);
}

void Rr_ConsumeNextObjectName(char Dst[RR_MAX_OBJECT_NAME_LENGTH])
{
    if (NextObjectName[0] != '\0')
    {
        for (uint32_t Index = 0; Index < RR_MAX_OBJECT_NAME_LENGTH; ++Index)
        {
            Dst[Index] = NextObjectName[Index];
        }
        NextObjectName[0] = '\0';
    }
}
