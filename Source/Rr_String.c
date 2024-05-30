#include "Rr_String.h"

#include "Rr_Memory.h"

#include <SDL3/SDL.h>

#include <stdlib.h>

static u32* UTF8ToUTF32(
    const char* CString,
    usize OptionalLength,
    u32* OutOldBuffer,
    usize OldLength,
    usize* OutNewLength)
{
    static u32 Buffer[2048] = { 0 };

    const u8 Ready = 128;
    const u8 Two = 192;
    const u8 Three = 224;
    const u8 Four = 240;
    const u8 Five = 248;

    usize SourceLength = OptionalLength > 0 ? OptionalLength : SDL_strlen(CString);
    if (SourceLength > 2048)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Exceeding max string length!");
        abort();
    }

    u8 Carry = 0;
    usize FinalIndex = 0;
    u32 FinalCharacter = 0;
    for (usize SourceIndex = 0; SourceIndex < SourceLength; ++SourceIndex)
    {
        if (Carry > 0)
        {
            Carry--;
            FinalCharacter |= (u8)((~Two & CString[SourceIndex]) << (Carry * 6));

            if (Carry == 0)
            {
                Buffer[FinalIndex] = FinalCharacter;
                FinalIndex++;
            }
        }
        else
        {
            if ((CString[SourceIndex] & Four) == Four)
            {
                FinalCharacter = (u8)(~Five & CString[SourceIndex]);
                FinalCharacter <<= 3 * 6;
                Carry = 3;
                continue;
            }
            else if ((CString[SourceIndex] & Three) == Three)
            {
                FinalCharacter = (u8)(~Four & CString[SourceIndex]);
                FinalCharacter <<= 2 * 6;
                Carry = 2;
                continue;
            }
            else if ((CString[SourceIndex] & Two) == Two)
            {
                FinalCharacter = (u8)(~Three & CString[SourceIndex]);
                FinalCharacter <<= 1 * 6;
                Carry = 1;
                continue;
            }
            else
            {
                Buffer[FinalIndex] = CString[SourceIndex] & ~Ready;
                FinalIndex++;
            }
        }
    }

    u32* Data;
    if (OutOldBuffer == NULL)
    {
        Data = (u32*)Rr_Malloc(sizeof(u32) * FinalIndex);
    }
    else if (OldLength >= FinalIndex)
    {
        Data = OutOldBuffer;
    }
    else
    {
        Data = (u32*)Rr_Realloc(OutOldBuffer, sizeof(u32) * FinalIndex);
    }

    SDL_memcpy((void*)Data, Buffer, sizeof(u32) * FinalIndex);

    *OutNewLength = FinalIndex;

    return Data;
}

Rr_String Rr_CreateString(const char* CString)
{
    Rr_String String;
    String.Data = UTF8ToUTF32(CString, 0, NULL, 0, &String.Length);

    return String;
}

Rr_String Rr_CreateEmptyString(usize Length)
{
    return (Rr_String){
        .Length = Length,
        .Data = (u32*)Rr_Calloc(Length, sizeof(u32))
    };
}

void Rr_SetString(Rr_String* String, const char* CString, usize OptionalLength)
{
    if (String == NULL)
    {
        return;
    }

    String->Data = UTF8ToUTF32(CString, OptionalLength, String->Data, String->Length, &String->Length);
}

void Rr_DestroyString(const Rr_String* String)
{
    if (String->Data != NULL)
    {
        Rr_Free((void*)String->Data);
    }
}
