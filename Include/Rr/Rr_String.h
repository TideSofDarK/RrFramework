#pragma once

#include <Rr/Rr_Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_String Rr_String;
struct Rr_String
{
    uint32_t *Data;
    size_t Length;
};

extern Rr_String Rr_CreateString(
    const char *CString,
    size_t LengthHint,
    Rr_Arena *Arena);

#define RR_STRING(CString, Arena) \
    Rr_CreateString(CString, sizeof(CString), Arena)

extern Rr_String Rr_CreateEmptyString(size_t Length, Rr_Arena *Arena);

extern void Rr_UpdateString(
    Rr_String *String,
    size_t MaxLength,
    const char *CString,
    size_t LengthHint);

#ifdef __cplusplus
}
#endif
