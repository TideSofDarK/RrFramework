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

#include <Rr/Rr_Defines.h>

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define RR_THREAD_LOCAL __declspec(thread)
#else
#define RR_THREAD_LOCAL __thread
#endif

#if (defined(_MSC_VER) && defined(_M_X64)) || defined(__SSE2__)
#include <immintrin.h>
#define RR_SPINLOCK_EMIT_PAUSE 1
#endif

typedef struct Rr_System Rr_System;
struct Rr_System
{
    bool Initialized;
    size_t PageSize;
    size_t AllocationGranularity;
    uint64_t PerformanceFrequency;
};

extern void Rr_InitSystem(void);

extern Rr_System *Rr_GetSystem(void);

extern uint64_t Rr_GetPerformanceCounter(void);

extern uint64_t Rr_GetPerformanceFrequency(void);

extern void *Rr_ReserveMemory(size_t Size);

extern void Rr_ReleaseMemory(void *Data, size_t Size);

extern bool Rr_CommitMemory(void *Data, size_t Size);

extern void Rr_DecommitMemory(void *Data, size_t Size);

extern void Rr_SleepNS(uint64_t Nanoseconds);

typedef struct Rr_AtomicInt
{
    int Value;
} Rr_AtomicInt;

static inline int Rr_LoadAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
#ifdef _MSC_VER
    /* TODO: Figure out *NoFence functions. */
    /* return _InterlockedOr((long *)&AtomicInt->Value, 0); */
    return AtomicInt->Value;
#else
    return __atomic_load_n(&AtomicInt->Value, __ATOMIC_RELAXED);
#endif
}

static inline int Rr_ExchangeAtomicAcquire(Rr_AtomicInt *AtomicInt, int Value)
{
#ifdef _MSC_VER
    return _InterlockedExchange(&AtomicInt->Value, Value);
#else
    return __atomic_exchange_n(&AtomicInt->Value, Value, __ATOMIC_ACQUIRE);
#endif
}

static inline void Rr_StoreAtomicRelease(Rr_AtomicInt *AtomicInt, int Value)
{
#ifdef _MSC_VER
    _InterlockedExchange(&AtomicInt->Value, Value);
#else
    __atomic_store_n(&AtomicInt->Value, Value, __ATOMIC_RELEASE);
#endif
}

static inline void Rr_StoreAtomicRelaxed(Rr_AtomicInt *AtomicInt, int Value)
{
#ifdef _MSC_VER
    AtomicInt->Value = Value;
#else
    __atomic_store_n(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
#endif
}

static inline int Rr_IncrementAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
#ifdef _MSC_VER
    return _InterlockedIncrement(&AtomicInt->Value);
#else
    return __atomic_fetch_add(&AtomicInt->Value, 1, __ATOMIC_RELAXED);
#endif
}

static inline int Rr_DecrementAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
#ifdef _MSC_VER
    return _InterlockedDecrement(&AtomicInt->Value);
#else
    return __atomic_fetch_sub(&AtomicInt->Value, 1, __ATOMIC_RELAXED);
#endif
}

typedef Rr_AtomicInt Rr_Spinlock;

static inline void Rr_LockSpinlock(Rr_Spinlock *Spinlock)
{
    for (;;)
    {
        if (!Rr_ExchangeAtomicAcquire(Spinlock, 1))
        {
            return;
        }
        while (Rr_LoadAtomicRelaxed(Spinlock))
        {
#ifdef RR_SPINLOCK_EMIT_PAUSE
            _mm_pause();
#endif
        }
    }
}

static inline bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock)
{
    bool Locked = !Rr_LoadAtomicRelaxed(Spinlock) &&
                  !Rr_ExchangeAtomicAcquire(Spinlock, 1);
    return Locked;
}

static inline void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock)
{
    Rr_StoreAtomicRelease(Spinlock, 0);
}
