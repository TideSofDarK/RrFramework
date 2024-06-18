#pragma once

#include "Rr_Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_String
{
    Rr_USize Length;
    Rr_U32* Data;
} Rr_String;

Rr_String Rr_CreateString(Rr_CString CString);
Rr_String Rr_CreateEmptyString(Rr_USize Length);
void Rr_SetString(Rr_String* String, Rr_CString CString, Rr_USize OptionalLength);
void Rr_DestroyString(Rr_String* String);

#ifdef __cplusplus
}
#endif
