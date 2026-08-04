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

#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Log.h>
#include <Rr/Rr_RHI.h>

typedef struct Rr_RHI Rr_RHI;
typedef struct Rr_Frame Rr_Frame;
typedef struct Rr_CommandPools Rr_CommandPools;
typedef struct Rr_Queue Rr_Queue;
typedef struct Rr_Device Rr_Device;

extern void Rr_InitRHI(Rr_Config const *Config);

extern void Rr_CleanupRHI(void);

extern Rr_CommandPools *Rr_AcquireCommandPools(void);

extern void Rr_ReleaseCommandPools(void);

extern Rr_Device *Rr_GetDevice(void);

extern void Rr_WaitIdle(void);

extern void Rr_SetSwapchainDirty(bool Dirty);

extern bool Rr_HandleSwapchainRecreated(void);

extern void Rr_NewFrame(void);

extern void Rr_DrawFrame(void);

extern Rr_Queue *Rr_GetQueue(Rr_QueueType QueueType);

extern Rr_Frame *Rr_GetCurrentFrame(void);

extern Rr_Arena *Rr_GetCurrentFrameArena(void);

extern void Rr_BeginFrameSection(char const *Name);

extern void Rr_EndFrameSection(char const *Name);

extern struct Rr_Profiler *Rr_GetFrameProfiler(void);

extern void Rr_ConsumeNextObjectName(char Dst[RR_MAX_OBJECT_NAME_LENGTH]);

static inline size_t Rr_GetFormatSize(Rr_Format Format)
{
    switch (Format)
    {
            /* INT */
        case RR_FORMAT_INT:
            return sizeof(int32_t);
        case RR_FORMAT_INT2:
            return sizeof(int32_t) * 2;
        case RR_FORMAT_INT3:
            return sizeof(int32_t) * 3;
        case RR_FORMAT_INT4:
            return sizeof(int32_t) * 4;
            /* UINT */
        case RR_FORMAT_UINT:
            return sizeof(uint32_t);
        case RR_FORMAT_UINT2:
            return sizeof(uint32_t) * 2;
        case RR_FORMAT_UINT3:
            return sizeof(uint32_t) * 3;
        case RR_FORMAT_UINT4:
            return sizeof(uint32_t) * 4;
            /* FLOAT */
        case RR_FORMAT_FLOAT:
            return sizeof(float);
        case RR_FORMAT_FLOAT2:
            return sizeof(float) * 2;
        case RR_FORMAT_FLOAT3:
            return sizeof(float) * 3;
        case RR_FORMAT_FLOAT4:
            return sizeof(float) * 4;
        default:
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid format!");
    }

    return 0;
}

extern Rr_RHI *gRHI;
