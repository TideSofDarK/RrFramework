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

#include "Rr_App.h"

#include "Rr_Arena.h"
#include "Rr_Platform.h"
#include "Rr_Renderer.h"
#include "Rr_UI.h"

#include <assert.h>

Rr_App *gApp = NULL;

static void Rr_CalculateDeltaTime(Rr_FrameTime *FrameTime)
{
    FrameTime->Last = FrameTime->Now;
    FrameTime->Now = Rr_GetPerformanceCounter();
    FrameTime->DeltaSeconds = (double)(FrameTime->Now - FrameTime->Last) /
                              (double)Rr_GetPerformanceFrequency();
}

static void Rr_SimulateVSync(Rr_FrameTime *FrameTime)
{
    uint64_t Interval = 1000000000 / FrameTime->TargetFramerate;
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

    FrameTime->TargetFramerate = (uint64_t)Rr_GetDisplayRefreshRate();
    FrameTime->StartTime = Now;
    FrameTime->Now = Now;

    FrameTime->InitTime = Now;
    assert(Rr_GetPerformanceFrequency() <= 1000000000);
    FrameTime->QPCToNS = 1000000000 / Rr_GetPerformanceFrequency();
}

static inline bool Rr_PollEvent(Rr_Event *Event)
{
    if (Rr_PollPlatformEvent(Event))
    {
        return true;
    }

    for (Rr_EventHiveIterator It = gApp->EventHive.Begin;
         It.Element != gApp->EventHive.End.Element;)
    {
        memcpy(Event, It.Element, sizeof(Rr_Event));
        Rr_RemoveFromEventHive(&gApp->EventHive, &It);
        return true;
    }

    return false;
}

static inline void Rr_DispatchEvents(Rr_AppConfig *Config)
{
    Rr_Event Event;

    if (gRenderer->Swapchain.RecreateEventPending)
    {
        Event.Type = RR_EVENT_TYPE_SWAPCHAIN_CREATED;

        if (Config->EventFunc != NULL)
        {
            gApp->EventFunc(&Event);
        }

        gRenderer->Swapchain.RecreateEventPending = false;
    }

    while (Rr_PollEvent(&Event))
    {
        if (Event.Type == RR_EVENT_TYPE_QUIT)
        {
            /* TODO: Should have an option to ignore it. */

            Rr_Quit();
        }

        Rr_ProcessUIEvent(&Event);

        if (Config->EventFunc != NULL)
        {
            gApp->EventFunc(&Event);
        }
    }
}

static RR_THREAD_LOCAL Rr_ThreadContext *ThreadContext = NULL;
static RR_THREAD_LOCAL bool IsMainThread = false;

void Rr_Run(Rr_AppConfig *Config)
{
    assert(gApp == NULL && "You shouldn't call Rr_Run() more than once!");
    assert(Config->Title != NULL);
    assert(Config->IterateFunc != NULL);

    IsMainThread = true;

    Rr_InitPlatform();
    Rr_InitPlatformLibrary(Config);

    Rr_InitThreadContext();

    Rr_Arena *Arena = ThreadContext->Arena;

    gApp = RR_ALLOC_TYPE(Arena, Rr_App);
    gApp->InitFunc = Config->InitFunc;
    gApp->EventFunc = Config->EventFunc;
    gApp->IterateFunc = Config->IterateFunc;
    gApp->CleanupFunc = Config->CleanupFunc;

    Rr_InitFrameTime(&gApp->FrameTime);

    Rr_InitRenderer(Config->Title);

    Rr_NewFrame();
    RR_BEGIN_FRAME_SECTION("Rr.MainLoop");

    /* NOTE: Order is very important! UI initialization will create GPU
     * resources so it must have access to the graph. User provided Init
     * function also must have access. */

    Rr_InitUI();

    Rr_NewUIFrame();

    if (gApp->InitFunc)
    {
        gApp->InitFunc();
    }

    Rr_ShowWindow();

    while (true)
    {
        Rr_DispatchEvents(Config);

        Rr_Vec2 MousePosition = Rr_GetMousePosition();
        gPlatform->MousePositionDelta =
            Rr_SubV2(MousePosition, gPlatform->LastMousePosition);
        gPlatform->LastMousePosition = MousePosition;

        Rr_DestroyScratch(gPlatform->EventScratch);

        Rr_BeginUI();

        gApp->IterateFunc();

        Rr_EndUI();

        Rr_DrawFrame();

        bool Minimized = Rr_IsWindowMinimized();

        if (gApp->FrameTime.EnableFrameLimiter || Minimized)
        {
            Rr_SimulateVSync(&gApp->FrameTime);
        }

        Rr_CalculateDeltaTime(&gApp->FrameTime);

        if (Rr_LoadAtomicRelaxed(&gApp->QuitRequested))
        {
            break;
        }

        RR_END_FRAME_SECTION("Rr.MainLoop");

        /* NOTE: The reason Rr_NewFrame() is called before event processing
         * is to allow it to use temporary frame arena to buffer
         * stuff such as text input. */

        Rr_NewFrame();
        RR_BEGIN_FRAME_SECTION("Rr.MainLoop");

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

    Rr_CleanupPlatformLibrary();
}

void Rr_InitThreadContext(void)
{
    Rr_InitScratchArena();

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    ThreadContext = RR_ALLOC(Arena, sizeof(Rr_ThreadContext));
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

void Rr_SetFrameLimiterEnabled(bool Enabled)
{
    gApp->FrameTime.EnableFrameLimiter = Enabled;
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
