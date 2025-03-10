#if defined(__linux__)

#include <Rr/Rr_Platform.h>

#include "Rr_Log.h"

#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>

static Rr_PlatformInfo PlatformInfo;

bool Rr_InitPlatform(void)
{
    PlatformInfo.PageSize = getpagesize();
    PlatformInfo.AllocationGranularity = PlatformInfo.PageSize;

    return true;
}

Rr_PlatformInfo *Rr_GetPlatformInfo(void)
{
    return &PlatformInfo;
}

void *Rr_ReserveMemory(size_t Size)
{
    return mmap(0, Size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void Rr_ReleaseMemory(void *Data, size_t Size)
{
    munmap(Data, Size);
}

bool Rr_CommitMemory(void *Data, size_t Size)
{
    if(mprotect(Data, Size, PROT_READ | PROT_WRITE))
    {
        return false;
    }
    mlock(Data, Size);
    munlock(Data, Size);
    return true;
}

void Rr_DecommitMemory(void *Data, size_t Size)
{
    madvise(Data, Size, MADV_DONTNEED);
    mprotect(Data, Size, PROT_NONE);
}

int Rr_GetAtomicInt(Rr_AtomicInt *AtomicInt)
{
    return __atomic_load_n(&AtomicInt->Value, __ATOMIC_SEQ_CST);
}

int Rr_SetAtomicInt(Rr_AtomicInt *AtomicInt, int Value)
{
    return __sync_lock_test_and_set(&AtomicInt->Value, Value);
}

bool Rr_TryLockSpinLock(Rr_SpinLock *SpinLock)
{
    return __sync_lock_test_and_set(SpinLock, 1) == 0;
}

void Rr_UnlockSpinLock(Rr_SpinLock *SpinLock)
{
    __sync_lock_release(SpinLock);
}

#else

static void Rr_Unused(void);

#endif
