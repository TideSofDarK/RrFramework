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

#include "Rr_Atomic.h"

typedef struct Rr_FrameTime Rr_FrameTime;
struct Rr_FrameTime
{
    /* Frame Limiter */

    uint32_t TargetFrameRate;
    uint32_t BackgroundFrameRate;
    uint64_t StartTime;

    /* Delta Time Calculation */

    uint64_t Last;
    uint64_t Now;
    double DeltaSeconds;

    uint64_t InitTime;
};

typedef struct Rr_App Rr_App;
struct Rr_App
{
    void (*InitFunc)(void);
    void (*EventFunc)(Rr_Event const *Event);
    void (*IterateFunc)(void);
    void (*CleanupFunc)(void);

    Rr_AtomicInt QuitRequested;

    Rr_FrameTime FrameTime;
};

extern Rr_FrameTime *Rr_GetFrameTime(void);
