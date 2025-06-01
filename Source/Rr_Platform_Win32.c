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

#include <Rr/Rr_Platform.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static Rr_PlatformInfo PlatformInfo;

bool Rr_InitPlatform()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    PlatformInfo.PageSize = SystemInfo.dwPageSize;
    PlatformInfo.AllocationGranularity = SystemInfo.dwAllocationGranularity;

    return true;
}

Rr_PlatformInfo *Rr_GetPlatformInfo()
{
    return &PlatformInfo;
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
    return VirtualAlloc(Data, Size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

void Rr_DecommitMemory(void *Data, size_t Size)
{
    VirtualFree(Data, Size, MEM_DECOMMIT);
}

int Rr_GetAtomicInt(Rr_AtomicInt *AtomicInt)
{
    return _InterlockedOr((long *)&AtomicInt->Value, 0);
}

int Rr_SetAtomicInt(Rr_AtomicInt *AtomicInt, int Value)
{
    return _InterlockedExchange((long *)&AtomicInt->Value, Value);
}

int Rr_AddAtomicInt(Rr_AtomicInt *AtomicInt, int Value)
{
    return _InterlockedExchangeAdd((long *)&AtomicInt->Value, Value);
}

bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock)
{
    return InterlockedExchange((long *)Spinlock, 1) == 0;
}

void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock)
{
    _ReadWriteBarrier();
    *Spinlock = 0;
}
