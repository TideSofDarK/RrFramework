#include "Rr/Rr_String.h"

#include "Rr_Memory.h"
#include "Rr_Log.h"

static Rr_U32*
Rr_UTF8ToUTF32(
    Rr_CString CString,
    Rr_USize OptionalLength,
    Rr_U32* OutOldBuffer,
    Rr_USize OldLength,
    Rr_USize* OutNewLength)
{
    static Rr_U32 Buffer[2048] = { 0 };

    Rr_U8 Ready = 128;
    Rr_U8 Two = 192;
    Rr_U8 Three = 224;
    Rr_U8 Four = 240;
    Rr_U8 Five = 248;

    Rr_USize SourceLength = OptionalLength > 0 ? OptionalLength : strlen(CString);
    if (SourceLength > 2048)
    {
        Rr_LogAbort("Exceeding max string length!");
    }

    Rr_U8 Carry = 0;
    Rr_USize FinalIndex = 0;
    Rr_U32 FinalCharacter = 0;
    for (Rr_USize SourceIndex = 0; SourceIndex < SourceLength; ++SourceIndex)
    {
        if (Carry > 0)
        {
            Carry--;
            FinalCharacter |= (Rr_U8)((~Two & CString[SourceIndex]) << (Carry * 6));

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
                FinalCharacter = (Rr_U8)(~Five & CString[SourceIndex]);
                FinalCharacter <<= 3 * 6;
                Carry = 3;
                continue;
            }
            else if ((CString[SourceIndex] & Three) == Three)
            {
                FinalCharacter = (Rr_U8)(~Four & CString[SourceIndex]);
                FinalCharacter <<= 2 * 6;
                Carry = 2;
                continue;
            }
            else if ((CString[SourceIndex] & Two) == Two)
            {
                FinalCharacter = (Rr_U8)(~Three & CString[SourceIndex]);
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

    Rr_U32* Data;
    if (OutOldBuffer == NULL)
    {
        Data = (Rr_U32*)Rr_Malloc(sizeof(Rr_U32) * FinalIndex);
    }
    else if (OldLength >= FinalIndex)
    {
        Data = OutOldBuffer;
    }
    else
    {
        Data = (Rr_U32*)Rr_Realloc(OutOldBuffer, sizeof(Rr_U32) * FinalIndex);
    }

    memcpy((void*)Data, Buffer, sizeof(Rr_U32) * FinalIndex);

    *OutNewLength = FinalIndex;

    return Data;
}

Rr_String
Rr_CreateString(Rr_CString CString)
{
    Rr_String String;
    String.Data = Rr_UTF8ToUTF32(CString, 0, NULL, 0, &String.Length);

    return String;
}

Rr_String
Rr_CreateEmptyString(Rr_USize Length)
{
    return (Rr_String){ .Length = Length, .Data = (Rr_U32*)Rr_Calloc(Length, sizeof(Rr_U32)) };
}

void
Rr_SetString(Rr_String* String, Rr_CString CString, Rr_USize OptionalLength)
{
    if (String == NULL)
    {
        return;
    }

    String->Data = Rr_UTF8ToUTF32(CString, OptionalLength, String->Data, String->Length, &String->Length);
}

void
Rr_DestroyString(Rr_String* String)
{
    if (String->Data != NULL)
    {
        Rr_Free((void*)String->Data);
    }
}
