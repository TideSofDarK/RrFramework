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

#include <Rr/Rr_Input.h>
#include <Rr/Rr_Memory.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_vulkan.h>

#include <assert.h>

Rr_App *gApp = NULL;

static void Rr_CalculateDeltaTime(Rr_FrameTime *FrameTime)
{
    FrameTime->Last = FrameTime->Now;
    FrameTime->Now = SDL_GetPerformanceCounter();
    FrameTime->DeltaSeconds = (double)(FrameTime->Now - FrameTime->Last) /
                              (double)SDL_GetPerformanceFrequency();
}

static void Rr_CalculateFPS(Rr_FrameTime *FrameTime)
{
    FrameTime->PerformanceCounter.Frames++;
    uint64_t CurrentTime = SDL_GetPerformanceCounter();
    if (CurrentTime - FrameTime->PerformanceCounter.StartTime >=
        FrameTime->PerformanceCounter.UpdateFrequency)
    {
        double Elapsed =
            (double)(CurrentTime - FrameTime->PerformanceCounter.StartTime) /
            FrameTime->PerformanceCounter.CountPerSecond;
        FrameTime->PerformanceCounter.FPS =
            (double)FrameTime->PerformanceCounter.Frames / Elapsed;
        FrameTime->PerformanceCounter.StartTime = CurrentTime;
        FrameTime->PerformanceCounter.Frames = 0;
    }
}

static void Rr_SimulateVSync(Rr_FrameTime *FrameTime)
{
    uint64_t Interval = SDL_MS_TO_NS(1000) / FrameTime->TargetFramerate;
    uint64_t Now = SDL_GetTicksNS();
    uint64_t Elapsed = Now - FrameTime->StartTime;

    if (Elapsed < Interval)
    {
        SDL_DelayNS(Interval - Elapsed);
        Now = SDL_GetTicksNS();
    }

    Elapsed = Now - FrameTime->StartTime;

    if (!FrameTime->StartTime || Elapsed > SDL_MS_TO_NS(1000))
    {
        FrameTime->StartTime = Now;
    }
    else
    {
        FrameTime->StartTime += (Elapsed / Interval) * Interval;
    }
}

static void Rr_InitFrameTime(Rr_FrameTime *FrameTime, SDL_Window *Window)
{
#ifdef RR_PERFORMANCE_COUNTER
    FrameTime->PerformanceCounter.StartTime = SDL_GetPerformanceCounter();
    FrameTime->PerformanceCounter.UpdateFrequency =
        SDL_GetPerformanceFrequency() / 2;
    FrameTime->PerformanceCounter.CountPerSecond =
        (double)SDL_GetPerformanceFrequency();
#endif

    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(Window);
    const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(DisplayID);
    FrameTime->TargetFramerate = (uint64_t)Mode->refresh_rate;
    FrameTime->StartTime = SDL_GetTicksNS();
    FrameTime->Now = SDL_GetPerformanceCounter();
}

static Rr_IntVec2 Rr_GetDefaultWindowSize(void)
{
    Rr_IntVec2 DisplaySize = Rr_GetDisplaySize();

    float ScaleFactor = 0.75f;

    return (Rr_IntVec2){ .Width = (int32_t)(DisplaySize.Width * ScaleFactor),
                         .Height =
                             (int32_t)(DisplaySize.Height * ScaleFactor) };
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
    assert(Config->InitFunc != NULL);
    assert(Config->IterateFunc != NULL);
    assert(Config->CleanupFunc != NULL);

    Rr_InitPlatform();

    SDL_SetAppMetadata(Config->Title, Config->Version, Config->Package);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gApp = RR_ALLOC_TYPE(Arena, Rr_App);
    gApp->Arena = Arena;

    gApp->Config = Config;
    gApp->Window = SDL_CreateWindow(
        Config->Title,
        0,
        0,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
            SDL_WINDOW_HIGH_PIXEL_DENSITY);
    gApp->UserData = Config->UserData;

    Rr_SetWindowSize(Rr_GetDefaultWindowSize());

    Rr_SetScratchTLS(&gApp->ScratchArenaTLS);

    Rr_InitScratch(RR_MAIN_THREAD_SCRATCH_ARENA_SIZE);

    Rr_InitFrameTime(&gApp->FrameTime, gApp->Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_StartTextInput(gApp->Window);

    Rr_InitRenderer();

    Rr_InitUI();

    /* NOTE: Call these early so user-provided Init function may access Graph
     * and UI. */

    Rr_NewFrame();
    Rr_NewUIFrame();

    Config->InitFunc(gApp->UserData);

    SDL_ShowWindow(gApp->Window);

    while (true)
    {
        for (Rr_Event Event; Rr_PollEvent(&Event);)
        {
            Rr_ProcessUIEvent(&Event);

            switch (Event.Type)
            {
                case RR_EVENT_TYPE_QUIT:
                {
                    Rr_SetAtomicInt(&gApp->QuitRequested, true);
                    break;
                }
                break;
                default:
                    break;
            }

            if (Config->EventFunc != NULL)
            {
                Config->EventFunc(gApp->UserData, &Event);
            }
        }

        Rr_BeginUI();

        gApp->Config->IterateFunc(gApp->UserData);

        Rr_EndUI();

        Rr_DrawFrame();

#ifdef RR_PERFORMANCE_COUNTER
        Rr_CalculateFPS(&gApp->FrameTime);
#endif

        bool Minimized =
            SDL_GetWindowFlags(gApp->Window) & SDL_WINDOW_MINIMIZED;
        if (gApp->FrameTime.EnableFrameLimiter || Minimized)
        {
            Rr_SimulateVSync(&gApp->FrameTime);
        }

        Rr_CalculateDeltaTime(&gApp->FrameTime);

        if (Rr_GetAtomicInt(&gApp->QuitRequested))
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

    gApp->Config->CleanupFunc(gApp->UserData);

    Rr_CleanupUI();

    Rr_CleanupRenderer();

    SDL_CleanupTLS();

    SDL_DestroyWindow(gApp->Window);

    Rr_DestroyArena(gApp->Arena);

    SDL_Quit();
}

void Rr_SetFrameLimiterEnabled(bool Enabled)
{
    gApp->FrameTime.EnableFrameLimiter = Enabled;
}

double Rr_GetFramesPerSecond(void)
{
    return (float)gApp->FrameTime.PerformanceCounter.FPS;
}

static bool Rr_IsAnyFullscreen(void)
{
    return (SDL_GetWindowFlags(gApp->Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_ToggleFullscreen(void)
{
    SDL_SetWindowFullscreen(gApp->Window, !Rr_IsAnyFullscreen());
}

double Rr_GetDeltaSeconds(void)
{
    return gApp->FrameTime.DeltaSeconds;
}

double Rr_GetTimeSeconds(void)
{
    return (double)SDL_GetTicks() / 1000.0;
}

uint64_t Rr_GetTimeMS(void)
{
    return SDL_GetTicks();
}

void Rr_SetRelativeMouseMode(bool IsRelative)
{
    SDL_SetWindowRelativeMouseMode(gApp->Window, IsRelative ? true : false);
}

Rr_Event *Rr_AddEvent(void)
{
    Rr_EventHiveIterator It =
        Rr_PushEventIntoHive(&gApp->EventHive, gApp->Arena);
    return It.Element;
}
