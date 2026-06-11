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

#if defined(RR_MSVC)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>
#elif defined(RR_X86)
#include <immintrin.h>
#endif

static inline void *Rr_ExchangeAtomicPointerRelaxed(void **Pointer, void *Value)
{
#if defined(RR_MSVC_X86)
    return _InterlockedExchangePointer(Pointer, Value);
#elif defined(RR_MSVC_ARM)
    return _InterlockedExchangePointer_nf(Pointer, Value);
#else
    return __atomic_exchange_n(Pointer, Value, __ATOMIC_RELAXED);
#endif
}

/* I have tested some stuff on godbolt.org and learned that with MSVC + x86:
 * Load-relaxed and store-relaxed don't need intrinsics.
 * Load-acquire and store-release don't need intrinsics.
 * _HLEAcquire/_HLERelease can be ignored since it's an obscure feature and GCC
 * doesn't emit xacquire/xrelease by default. */

/* I couldn't look into _acq/_rel/_nf disassembly as godbolt.org
 * shows them as function calls. With no access to an ARM box right now I'm not
 * really motivated to dig deeper. */

/* 64-Bit Signed Atomic Integer */

typedef struct Rr_AtomicInt Rr_AtomicInt;
struct Rr_AtomicInt
{
#if defined(RR_MSVC)
    LONG64 Value;
#else
    int64_t Value;
#endif
};

static inline int64_t Rr_LoadAtomicIntRelaxed(Rr_AtomicInt *AtomicInt)
{
#if defined(RR_MSVC_X86)
    return (int64_t)AtomicInt->Value;
#elif defined(RR_MSVC_ARM)
    return (int64_t)_InterlockedOr64_nf(&AtomicInt->Value, (LONG64)0);
#else
    return __atomic_load_n(&AtomicInt->Value, __ATOMIC_RELAXED);
#endif
}

static inline int64_t Rr_LoadAtomicIntAcquire(Rr_AtomicInt *AtomicInt)
{
#if defined(RR_MSVC_X86)
    return (int64_t)AtomicInt->Value;
#elif defined(RR_MSVC_ARM)
    return (int64_t)_InterlockedOr64_acq(&AtomicInt->Value, (LONG64)0);
#else
    return __atomic_load_n(&AtomicInt->Value, __ATOMIC_ACQUIRE);
#endif
}

static inline void Rr_StoreAtomicIntRelaxed(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(RR_MSVC_X86)
    AtomicInt->Value = (LONG64)Value;
#elif defined(RR_MSVC_ARM)
    _InterlockedExchange64_nf(&AtomicInt->Value, (LONG64)Value);
#else
    __atomic_store_n(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
#endif
}

static inline void Rr_StoreAtomicIntRelease(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(RR_MSVC_X86)
    AtomicInt->Value = (LONG64)Value;
#elif defined(RR_MSVC_ARM)
    _InterlockedExchange64_rel(&AtomicInt->Value, (LONG64)Value);
#else
    __atomic_store_n(&AtomicInt->Value, Value, __ATOMIC_RELEASE);
#endif
}

static inline int64_t Rr_ExchangeAtomicIntRelaxed(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(RR_MSVC_X86)
    return (int64_t)_InterlockedExchange64(&AtomicInt->Value, (LONG64)Value);
#elif defined(RR_MSVC_ARM)
    return (int64_t)_InterlockedExchange64_nf(&AtomicInt->Value, (LONG64)Value);
#else
    return __atomic_exchange_n(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
#endif
}

static inline int64_t Rr_ExchangeAtomicIntAcquire(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(RR_MSVC_X86)
    return (int64_t)_InterlockedExchange64(&AtomicInt->Value, (LONG64)Value);
#elif defined(RR_MSVC_ARM)
    return (
        int64_t)_InterlockedExchange64_acq(&AtomicInt->Value, (LONG64)Value);
#else
    return __atomic_exchange_n(&AtomicInt->Value, Value, __ATOMIC_ACQUIRE);
#endif
}

static inline int64_t Rr_AddAtomicIntRelaxed(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(RR_MSVC_X86)
    return (int64_t)_InterlockedExchangeAdd64(&AtomicInt->Value, (LONG64)Value);
#elif defined(RR_MSVC_ARM)
    return (
        int64_t)_InterlockedExchangeAdd64_nf(&AtomicInt->Value, (LONG64)Value);
#else
    return __atomic_fetch_add(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
#endif
}

static inline int64_t Rr_IncrementAtomicIntRelaxed(Rr_AtomicInt *AtomicInt)
{
#if defined(RR_MSVC_X86)
    return (int64_t)_InterlockedIncrement64(&AtomicInt->Value);
#elif defined(RR_MSVC_ARM)
    return (int64_t)_InterlockedIncrement64_nf(&AtomicInt->Value);
#else
    return __atomic_fetch_add(&AtomicInt->Value, 1, __ATOMIC_RELAXED);
#endif
}

static inline int64_t Rr_DecrementAtomicIntRelaxed(Rr_AtomicInt *AtomicInt)
{
#if defined(RR_MSVC_X86)
    return (int64_t)_InterlockedDecrement64(&AtomicInt->Value);
#elif defined(RR_MSVC_ARM)
    return (int64_t)_InterlockedDecrement64_nf(&AtomicInt->Value);
#else
    return __atomic_fetch_sub(&AtomicInt->Value, 1, __ATOMIC_RELAXED);
#endif
}

static inline bool Rr_CompareExchangeAtomicIntRelaxed(
    Rr_AtomicInt *AtomicInt,
    int64_t *Expected,
    int64_t Desired)
{
#if defined(RR_MSVC_X86)
    int64_t OldExpected = *Expected;
    *Expected = (int64_t)_InterlockedCompareExchange64(
        &AtomicInt->Value,
        (LONG64)Desired,
        (LONG64)*Expected);

    return OldExpected == *Expected;
#elif defined(RR_MSVC_ARM)
    int64_t OldExpected = *Expected;
    *Expected = (int64_t)_InterlockedCompareExchange64_nf(
        &AtomicInt->Value,
        (LONG64)Desired,
        (LONG64)*Expected);

    return OldExpected == *Expected;
#else
    return __atomic_compare_exchange_n(
        &AtomicInt->Value,
        Expected,
        Desired,
        true,
        __ATOMIC_RELAXED,
        __ATOMIC_RELAXED);
#endif
}

static inline int64_t Rr_MinAtomicIntRelaxed(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(__has_builtin)
#if __has_builtin(__atomic_fetch_min)
    return __atomic_fetch_min(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
#endif
#endif

    int64_t OldValue = Rr_LoadAtomicIntRelaxed(AtomicInt);
    int64_t NewValue = OldValue < Value ? OldValue : Value;
    while (!Rr_CompareExchangeAtomicIntRelaxed(AtomicInt, &OldValue, NewValue))
    {
        NewValue = OldValue < Value ? OldValue : Value;
    }

    return OldValue;
}

static inline int64_t Rr_MaxAtomicIntRelaxed(
    Rr_AtomicInt *AtomicInt,
    int64_t Value)
{
#if defined(__has_builtin)
#if __has_builtin(__atomic_fetch_max)
    return __atomic_fetch_max(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
#endif
#endif

    int64_t OldValue = Rr_LoadAtomicIntRelaxed(AtomicInt);
    int64_t NewValue = OldValue > Value ? OldValue : Value;
    while (!Rr_CompareExchangeAtomicIntRelaxed(AtomicInt, &OldValue, NewValue))
    {
        NewValue = OldValue > Value ? OldValue : Value;
    }

    return OldValue;
}

/* Spinlock */

typedef Rr_AtomicInt Rr_Spinlock;

static inline void Rr_LockSpinlock(Rr_Spinlock *Spinlock)
{
    for (;;)
    {
        if (!Rr_ExchangeAtomicIntAcquire(Spinlock, 1))
        {
            return;
        }
        while (Rr_LoadAtomicIntRelaxed(Spinlock))
        {
#if defined(RR_X86)
            _mm_pause();
#elif defined(RR_MSVC_ARM)
            __isb(0);
#else
            __asm__ __volatile__("isb\n");
#endif
        }
    }
}

static inline bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock)
{
    bool Locked = !Rr_LoadAtomicIntRelaxed(Spinlock) &&
                  !Rr_ExchangeAtomicIntAcquire(Spinlock, 1);
    return Locked;
}

static inline void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock)
{
    Rr_StoreAtomicIntRelease(Spinlock, 0);
}
