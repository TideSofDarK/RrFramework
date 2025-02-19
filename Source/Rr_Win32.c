#if defined(_WIN32)

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
    return _InterlockedExchange((long *)&AtomicInt->Value, v);
}

bool Rr_TryLockSpinLock(Rr_SpinLock *SpinLock)
{
    return InterlockedExchange((long *)SpinLock, 1) == 0;
}

void Rr_UnlockSpinLock(Rr_SpinLock *SpinLock)
{
    _ReadWriteBarrier();
    *SpinLock = 0;
}

#else

static void Rr_Unused(void);

#endif
