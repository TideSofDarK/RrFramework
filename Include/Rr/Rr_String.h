#pragma once

#include <Rr/Rr_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_String Rr_String;
struct Rr_String
{
    size_t Length;
    uint32_t *Data;
};

Rr_String Rr_CreateString(const char *CString);

Rr_String Rr_CreateEmptyString(size_t Length);

void Rr_SetString(Rr_String *String, const char *CString, size_t OptionalLength);

void Rr_DestroyString(Rr_String *String);

#ifdef __cplusplus
}
#endif
