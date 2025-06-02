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

#pragma once

#include <Rr/Rr_App.h>

#include <Rr/Rr_Memory.h>

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_video.h>

#define RR_HIVE_TYPE      Rr_Event
#define RR_HIVE_TYPE_NAME Event
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

struct Rr_Renderer;

typedef union Rr_Object Rr_Object;

typedef struct Rr_FrameTime Rr_FrameTime;
struct Rr_FrameTime
{
#ifdef RR_PERFORMANCE_COUNTER
    struct
    {
        double FPS;
        uint64_t Frames;
        uint64_t StartTime;
        uint64_t UpdateFrequency;
        double CountPerSecond;
    } PerformanceCounter;
#endif

    /* Frame Limiter */

    uint64_t TargetFramerate;
    uint64_t StartTime;
    bool EnableFrameLimiter;

    /* Delta Time Calculation */

    uint64_t Last;
    uint64_t Now;
    double DeltaSeconds;
};

typedef struct Rr_App Rr_App;
struct Rr_App
{
    Rr_AppConfig *Config;
    void *UserData;

    Rr_FrameTime FrameTime;

    SDL_TLSID ScratchArenaTLS;
    Rr_Window Window;
    Rr_AtomicInt QuitRequested;

    Rr_EventHive EventHive;

    Rr_Arena *Arena;
};

extern Rr_Event *Rr_AddEvent(void);

extern Rr_App *gApp;
