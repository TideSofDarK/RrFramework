#include "Rr/Rr_String.h"

#include "Rr_Log.h"
#include "Rr_Memory.h"

static uint32_t *Rr_UTF8ToUTF32(
    const char *CString,
    size_t OptionalLength,
    uint32_t *OutOldBuffer,
    size_t OldLength,
    size_t *OutNewLength)
{
    static uint32_t Buffer[2048] = { 0 };

    uint8_t Ready = 128;
    uint8_t Two = 192;
    uint8_t Three = 224;
    uint8_t Four = 240;
    uint8_t Five = 248;

    size_t SourceLength = OptionalLength > 0 ? OptionalLength : strlen(CString);
    if(SourceLength > 2048)
    {
        Rr_LogAbort("Exceeding max string length!");
    }

    uint8_t Carry = 0;
    size_t FinalIndex = 0;
    uint32_t FinalCharacter = 0;
    for(size_t SourceIndex = 0; SourceIndex < SourceLength; ++SourceIndex)
    {
        if(Carry > 0)
        {
            Carry--;
            FinalCharacter |= (uint8_t)((~Two & CString[SourceIndex]) << (Carry * 6));

            if(Carry == 0)
            {
                Buffer[FinalIndex] = FinalCharacter;
                FinalIndex++;
            }
        }
        else
        {
            if((CString[SourceIndex] & Four) == Four)
            {
                FinalCharacter = (uint8_t)(~Five & CString[SourceIndex]);
                FinalCharacter <<= 3 * 6;
                Carry = 3;
                continue;
            }
            else if((CString[SourceIndex] & Three) == Three)
            {
                FinalCharacter = (uint8_t)(~Four & CString[SourceIndex]);
                FinalCharacter <<= 2 * 6;
                Carry = 2;
                continue;
            }
            else if((CString[SourceIndex] & Two) == Two)
            {
                FinalCharacter = (uint8_t)(~Three & CString[SourceIndex]);
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

    uint32_t *Data;
    if(OutOldBuffer == NULL)
    {
        Data = (uint32_t *)Rr_Malloc(sizeof(uint32_t) * FinalIndex);
    }
    else if(OldLength >= FinalIndex)
    {
        Data = OutOldBuffer;
    }
    else
    {
        Data = (uint32_t *)Rr_Realloc(OutOldBuffer, sizeof(uint32_t) * FinalIndex);
    }

    memcpy((void *)Data, Buffer, sizeof(uint32_t) * FinalIndex);

    *OutNewLength = FinalIndex;

    return Data;
}

Rr_String Rr_CreateString(const char *CString)
{
    Rr_String String;
    String.Data = Rr_UTF8ToUTF32(CString, 0, NULL, 0, &String.Length);

    return String;
}

Rr_String Rr_CreateEmptyString(size_t Length)
{
    return (Rr_String){ .Length = Length, .Data = (uint32_t *)Rr_Calloc(Length, sizeof(uint32_t)) };
}

void Rr_SetString(Rr_String *String, const char *CString, size_t OptionalLength)
{
    if(String == NULL)
    {
        return;
    }

    String->Data = Rr_UTF8ToUTF32(CString, OptionalLength, String->Data, String->Length, &String->Length);
}

void Rr_DestroyString(Rr_String *String)
{
    if(String->Data != NULL)
    {
        Rr_Free((void *)String->Data);
    }
}
