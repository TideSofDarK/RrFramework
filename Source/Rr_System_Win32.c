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

#include "Rr_System.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_SYSTEM
#include "Rr_LogMacro.h"

#include "Rr_Thread.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void Rr_InitSystem(void)
{
    Rr_System *System = Rr_GetSystem();
    if (System->Initialized)
    {
        return;
    }

    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    System->PageSize = (size_t)SystemInfo.dwPageSize;
    System->AllocationGranularity = (size_t)SystemInfo.dwAllocationGranularity;

    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    System->PerformanceFrequency = (uint64_t)Frequency.QuadPart;

    System->Initialized = true;
}

uint64_t Rr_GetPerformanceCounter(void)
{
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);

    return (uint64_t)Counter.QuadPart;
}

uint64_t Rr_GetPerformanceFrequency(void)
{
    return Rr_GetSystem()->PerformanceFrequency;
}

void *Rr_ReserveMemory(size_t Size)
{
    return VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
}

void Rr_ReleaseMemory(void *Data, size_t Size)
{
    VirtualFree(Data, 0, MEM_RELEASE);
}

bool Rr_CommitMemory(void *Data, size_t Size)
{
    if (VirtualAlloc(Data, Size, MEM_COMMIT, PAGE_READWRITE) == NULL)
    {
        /* RR_LOG("error %d", GetLastError()); */

        return false;
    }

    return true;
}

void Rr_DecommitMemory(void *Data, size_t Size)
{
#if defined(RR_MSVC)
#pragma warning(push)
#pragma warning(disable : 6250)
#endif
    VirtualFree(Data, Size, MEM_DECOMMIT);
#if defined(RR_MSVC)
#pragma warning(pop)
#endif
}

static HANDLE Rr_GetWaitableEvent(void)
{
    static RR_THREAD_LOCAL HANDLE Event = NULL;
    if (!Event)
    {
        Event = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    return Event;
}

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#endif

static HANDLE Rr_GetWaitableTimer(void)
{
    static RR_THREAD_LOCAL HANDLE Timer = NULL;
    if (!Timer)
    {
        Timer = CreateWaitableTimerExW(
            NULL,
            NULL,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
    }

    return Timer;
}

void Rr_SleepNS(uint64_t Nanoseconds)
{
    HANDLE Timer = Rr_GetWaitableTimer();
    if (Timer)
    {
        LARGE_INTEGER DueTime;
        DueTime.QuadPart = -((LONGLONG)Nanoseconds / 100);
        if ((SetWaitableTimerEx(Timer, &DueTime, 0, NULL, NULL, NULL, 0)) ||
            SetWaitableTimer(Timer, &DueTime, 0, NULL, NULL, 0))
        {
            WaitForSingleObject(Timer, INFINITE);
        }

        return;
    }

    const uint64_t MaxDelay = 0xffffffffLL * 1000000;
    if (Nanoseconds > MaxDelay)
    {
        Nanoseconds = MaxDelay;
    }
    const DWORD Delay = (DWORD)(Nanoseconds / 1000000);

    HANDLE Event = Rr_GetWaitableEvent();
    if (Event)
    {
        WaitForSingleObjectEx(Event, Delay, FALSE);

        return;
    }

    Sleep(Delay);
}
