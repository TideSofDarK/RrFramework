#include <Rr/Rr_Platform.h>

#include "Rr_Log.h"

#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>

static Rr_PlatformInfo PlatformInfo;

bool Rr_InitPlatform(void)
{
    PlatformInfo.PageSize = getpagesize();

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
