#pragma once

#include <Rr/Rr_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_PlatformInfo Rr_PlatformInfo;
struct Rr_PlatformInfo
{
    int PageSize;
};

extern bool Rr_InitPlatform(void);

extern Rr_PlatformInfo *Rr_GetPlatformInfo(void);

extern void *Rr_ReserveMemory(size_t Size);

extern void Rr_ReleaseMemory(void *Data, size_t Size);

extern bool Rr_CommitMemory(void *Data, size_t Size);

extern void Rr_DecommitMemory(void *Data, size_t Size);

typedef int Rr_SpinLock;

extern bool Rr_TryLockSpinLock(Rr_SpinLock *SpinLock);

extern void Rr_LockSpinLock(Rr_SpinLock *SpinLock);

extern void Rr_UnlockSpinLock(Rr_SpinLock *SpinLock);

#ifdef __cplusplus
}
#endif
