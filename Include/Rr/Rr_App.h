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

typedef struct Rr_AppConfig Rr_AppConfig;
struct Rr_AppConfig
{
    const char *Title;
    Rr_WindowFlags WindowFlags;
    void (*InitFunc)(void);
    void (*EventFunc)(Rr_Event const *Event);
    void (*IterateFunc)(void);
    void (*CleanupFunc)(void);
};

#ifdef __cplusplus
extern "C" {
#endif

extern void RR_CC Rr_Run(Rr_AppConfig *Config);

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
