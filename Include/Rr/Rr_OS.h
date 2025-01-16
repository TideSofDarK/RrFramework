#pragma once

#include <Rr/Rr_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *Rr_ReserveMemory(size_t Size);
extern void *Rr_ReleaseMemory(void *Data, size_t Size);
extern void *Rr_CommitMemory(void *Data, size_t Size);
extern void *Rr_DecommitMemory(void *Data, size_t Size);

#ifdef __cplusplus
}
#endif
