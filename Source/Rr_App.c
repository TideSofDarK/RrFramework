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

#include "Rr_Platform.h"
#include "Rr_Renderer.h"
#include "Rr_UI.h"

#include <Rr/Rr_Memory.h>

#include <assert.h>
#include <threads.h>

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
    uint64_t Now = Rr_GetPerformanceCounter();
    uint64_t Elapsed = Now - FrameTime->StartTime;

    if (Elapsed < Interval)
    {
        thrd_sleep(
            &(struct timespec){ .tv_nsec = (long)(Interval - Elapsed) },
            NULL);
        Now = Rr_GetPerformanceCounter();
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

void Rr_Run(Rr_AppConfig *Config)
{
    assert(gApp == NULL && "You shouldn't call Rr_Run() more than once!");
    assert(Config->Title != NULL);
    assert(Config->IterateFunc != NULL);

    Rr_InitPlatform();
    Rr_InitPlatformLibrary(Config);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gApp = RR_ALLOC_TYPE(Arena, Rr_App);
    gApp->Arena = Arena;

    gApp->InitFunc = Config->InitFunc;
    gApp->EventFunc = Config->EventFunc;
    gApp->IterateFunc = Config->IterateFunc;
    gApp->CleanupFunc = Config->CleanupFunc;

    Rr_InitScratchArena();

    Rr_InitFrameTime(&gApp->FrameTime);

    Rr_InitRenderer(Config->Title);

    /* NOTE: Order is very important! UI initialization will create GPU
     * resources so it must have access to the graph. User provided Init
     * function also must have access. */

    Rr_NewFrame();

    Rr_InitUI();

    Rr_NewUIFrame();

    if (gApp->InitFunc)
    {
        gApp->InitFunc();
    }

    Rr_ShowWindow();

    while (true)
    {
        for (Rr_Event Event; Rr_PollEvent(&Event);)
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

        /* NOTE: The reason Rr_NewFrame() is called before processing events
         * is to allow event processing use temporary frame arena to buffer
         * stuff such as text input.
         * Currently the UI relies on it. */

        Rr_NewFrame();
        Rr_NewUIFrame();
    }

    Rr_WaitIdle();

    if (gApp->CleanupFunc)
    {
        gApp->CleanupFunc();
    }

    Rr_CleanupUI();

    Rr_CleanupRenderer();

    Rr_CleanupScratchArena();

    Rr_DestroyArena(gApp->Arena);

    Rr_CleanupPlatformLibrary();
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

void Rr_InitThreadContext(void)
{
    Rr_InitScratchArena();
}

void Rr_CleanupThreadContext(void)
{
    Rr_CleanupScratchArena();
}

Rr_Event *Rr_AddEvent(void)
{
    Rr_EventHiveIterator It =
        Rr_PushEventIntoHive(&gApp->EventHive, gApp->Arena);
    return It.Element;
}
