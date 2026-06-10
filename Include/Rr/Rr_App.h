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

#ifndef RR_APP_H
#define RR_APP_H

#include <Rr/Rr_Platform.h>

typedef void (*Rr_InitFunc)(void);
typedef void (*Rr_EventFunc)(Rr_Event const *);
typedef void (*Rr_IterateFunc)(void);
typedef void (*Rr_CleanupFunc)(void);

typedef struct Rr_Config Rr_Config;
struct Rr_Config
{
    const char *Title;

    const char *WindowTitle;
    Rr_IntVec2 WindowSize; /* TODO */
    Rr_WindowFlags WindowFlags;

    Rr_InitFunc InitFunc;
    Rr_EventFunc EventFunc;
    Rr_IterateFunc IterateFunc;
    Rr_CleanupFunc CleanupFunc;
};

#ifdef __cplusplus
extern "C" {
#endif

extern void RR_CC Rr_Run(Rr_Config *Config);

extern void RR_CC Rr_SetTargetFrameRate(uint32_t FramesPerSecond);

extern void RR_CC Rr_SetBackgroundFrameRate(uint32_t FramesPerSecond);

extern double RR_CC Rr_GetFramesPerSecond(void);

extern double RR_CC Rr_GetDeltaSeconds(void);

extern double RR_CC Rr_GetTimeSeconds(void);

extern uint64_t RR_CC Rr_GetTimeMS(void);

extern uint64_t RR_CC Rr_GetTimeNS(void);

extern void RR_CC Rr_Quit(void);

extern bool RR_CC Rr_QuitRequested(void);

#ifdef __cplusplus
}
#endif

#endif
