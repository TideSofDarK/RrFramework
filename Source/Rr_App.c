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

#include "Rr_App.h"

#include "Rr_Platform.h"
#include "Rr_Renderer.h"
#include "Rr_UI.h"

#include <assert.h>

Rr_App *gApp = NULL;

static RR_THREAD_LOCAL Rr_ThreadContext *ThreadContext = NULL;
static RR_THREAD_LOCAL bool IsMainThread = false;

static void Rr_CalculateDeltaTime(Rr_FrameTime *FrameTime)
{
    FrameTime->Last = FrameTime->Now;
    FrameTime->Now = Rr_GetPerformanceCounter();
    FrameTime->DeltaSeconds = (double)(FrameTime->Now - FrameTime->Last) /
                              (double)Rr_GetPerformanceFrequency();
}

static void Rr_SimulateVSync(Rr_FrameTime *FrameTime, uint32_t FrameRate)
{
    uint64_t Interval = 1000000000 / FrameRate;
    uint64_t Now = Rr_GetTimeNS();
    uint64_t Elapsed = Now - FrameTime->StartTime;

    if (Elapsed < Interval)
    {
        Rr_SleepNS(Interval - Elapsed);
        Now = Rr_GetTimeNS();
    }

    Elapsed = Now - FrameTime->StartTime;

    if (!FrameTime->StartTime || Elapsed > 1000000000)
    {
        FrameTime->StartTime = Now;
    }
    else
    {
        FrameTime->StartTime += (Elapsed / Interval) * Interval;
    }
}

static void Rr_InitFrameTime(Rr_FrameTime *FrameTime)
{
    uint64_t Now = Rr_GetPerformanceCounter();

    FrameTime->TargetFrameRate = 0;
    /* FrameTime->BackgroundFrameRate = (uint64_t)Rr_GetDisplayRefreshRate(); */
    FrameTime->BackgroundFrameRate = 30;
    FrameTime->StartTime = Now;
    FrameTime->Now = Now;

    FrameTime->InitTime = Now;
    assert(Rr_GetPerformanceFrequency() <= 1000000000);
    FrameTime->QPCToNS = 1000000000 / Rr_GetPerformanceFrequency();
}

static inline void Rr_HandleEvent(Rr_Event const *Event)
{
    if (Event->Type == RR_EVENT_TYPE_QUIT)
    {
        /* TODO: Should have an option to ignore it. */

        Rr_Quit();
    }

    Rr_ProcessUIEvent(Event);

    if (gApp->EventFunc != NULL)
    {
        gApp->EventFunc(Event);
    }
}

static inline void Rr_DispatchEvents(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Event Event;

    if (gRenderer->Swapchain.RecreateEventPending)
    {
        Event.Type = RR_EVENT_TYPE_SWAPCHAIN_CREATED;

        if (gApp->EventFunc != NULL)
        {
            gApp->EventFunc(&Event);
        }

        gRenderer->Swapchain.RecreateEventPending = false;
    }

    while (Rr_PollPlatformEvent(&Event, Scratch.Arena))
    {
        Rr_HandleEvent(&Event);
    }

    for (Rr_EventHiveIterator It = gApp->EventHive.Begin;
         It.Element != gApp->EventHive.End.Element;
         Rr_AdvanceEventHiveIterator(&It))
    {
        Rr_HandleEvent(It.Element);
    }
    Rr_ClearEventHive(&gApp->EventHive);

    Rr_DestroyScratch(Scratch);
}

void Rr_Run(Rr_AppConfig *Config)
{
    assert(gApp == NULL && "You shouldn't call Rr_Run() more than once!");
    assert(Config->Title != NULL);
    assert(Config->IterateFunc != NULL);

    IsMainThread = true;

    Rr_InitSystem();
    Rr_InitPlatform(Config);

    Rr_InitThreadContext();

    Rr_Arena *Arena = ThreadContext->Arena;

    gApp = RR_ALLOC_TYPE(Rr_App, Arena);
    gApp->InitFunc = Config->InitFunc;
    gApp->EventFunc = Config->EventFunc;
    gApp->IterateFunc = Config->IterateFunc;
    gApp->CleanupFunc = Config->CleanupFunc;

    Rr_InitFrameTime(&gApp->FrameTime);

    Rr_InitRenderer(Config->Title);

    Rr_NewFrame();
    Rr_BeginFrameSection("Rr.MainLoop");

    /* NOTE: Order is important! UI initialization will create GPU
     * resources so it must have access to the graph.
     * User-provided 'InitFunc' also must have access. */

    Rr_InitUI();

    Rr_NewUIFrame();

    if (gApp->InitFunc)
    {
        gApp->InitFunc();
    }

    Rr_ShowWindow();

    while (true)
    {
        Rr_NewPlatformFrame();

        Rr_DispatchEvents();

        Rr_BeginUI();

        gApp->IterateFunc();

        Rr_EndUI();

        Rr_DrawFrame();

        bool Minimized = Rr_IsWindowMinimized();

        if (Minimized)
        {
            Rr_SimulateVSync(
                &gApp->FrameTime,
                gApp->FrameTime.BackgroundFrameRate);
        }
        else if (gApp->FrameTime.TargetFrameRate)
        {
            Rr_SimulateVSync(&gApp->FrameTime, gApp->FrameTime.TargetFrameRate);
        }

        Rr_CalculateDeltaTime(&gApp->FrameTime);

        if (Rr_LoadAtomicRelaxed(&gApp->QuitRequested))
        {
            break;
        }

        Rr_EndFrameSection("Rr.MainLoop");

        /* NOTE: The reason Rr_NewFrame() is called before event processing
         * is to allow it to use temporary frame arena to buffer
         * stuff such as text input. */

        Rr_NewFrame();
        Rr_BeginFrameSection("Rr.MainLoop");

        Rr_NewUIFrame();
    }

    Rr_WaitIdle();

    if (gApp->CleanupFunc)
    {
        gApp->CleanupFunc();
    }

    Rr_CleanupUI();

    Rr_CleanupRenderer();

    Rr_CleanupThreadContext();

    Rr_CleanupPlatform();
}

void Rr_InitThreadContext(void)
{
    Rr_InitScratchArena();

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    ThreadContext = RR_ALLOC(sizeof(Rr_ThreadContext), Arena);
    ThreadContext->Arena = Arena;
}

void Rr_CleanupThreadContext(void)
{
    if (!ThreadContext)
    {
        return;
    }

    if (!IsMainThread)
    {
        Rr_ReleaseCommandPools();
    }

    Rr_DestroyArena(ThreadContext->Arena);
    ThreadContext = NULL;

    Rr_CleanupScratchArena();
}

Rr_ThreadContext *Rr_GetThreadContext(void)
{
    return ThreadContext;
}

void Rr_SetTargetFrameRate(uint32_t FramesPerSecond)
{
    gApp->FrameTime.TargetFrameRate = FramesPerSecond;
}

void Rr_SetBackgroundFrameRate(uint32_t FramesPerSecond)
{
    gApp->FrameTime.BackgroundFrameRate = FramesPerSecond;
}

double Rr_GetDeltaSeconds(void)
{
    return gApp->FrameTime.DeltaSeconds;
}

double Rr_GetTimeSeconds(void)
{
    return (double)(Rr_GetTimeNS()) / 1000000000.0;
}

uint64_t Rr_GetTimeMS(void)
{
    return Rr_GetTimeNS() / 1000000;
}

uint64_t Rr_GetTimeNS(void)
{
    return (Rr_GetPerformanceCounter() - gApp->FrameTime.InitTime) *
           gApp->FrameTime.QPCToNS;
}

void Rr_Quit(void)
{
    Rr_StoreAtomicRelaxed(&gApp->QuitRequested, 1);
}

bool Rr_QuitRequested(void)
{
    return Rr_LoadAtomicRelaxed(&gApp->QuitRequested);
}

Rr_Event *Rr_AddEvent(void)
{
    Rr_EventHiveIterator It =
        Rr_PushEventIntoHive(&gApp->EventHive, ThreadContext->Arena);
    return It.Element;
}
