#include "Rr/Rr_String.h"

#include "Rr_Log.h"

#include <string.h>

static size_t Rr_UTF8ToUTF32(const char *CString, size_t Length, uint32_t *Buffer, size_t BufferLength)
{
    uint8_t Ready = 128;
    uint8_t Two = 192;
    uint8_t Three = 224;
    uint8_t Four = 240;
    uint8_t Five = 248;

    uint8_t Carry = 0;
    size_t FinalIndex = 0;
    uint32_t FinalCharacter = 0;
    for(size_t SourceIndex = 0; SourceIndex < Length; ++SourceIndex)
    {
        if(FinalIndex >= BufferLength)
        {
            break;
        }

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

    return FinalIndex;
}

Rr_String Rr_CreateString(const char *CString, size_t LengthHint, Rr_Arena *Arena)
{
    if(CString == NULL)
    {
        RR_ABORT("Attempting to parse NULL string!");
    }

    size_t SourceLength = LengthHint > 0 ? LengthHint : strlen(CString);

    uint32_t *Buffer = RR_ALLOC_NO_ZERO(Arena, sizeof(uint32_t) * SourceLength);

    size_t FinalLength = Rr_UTF8ToUTF32(CString, SourceLength, Buffer, SourceLength);

    Rr_PopArena(Arena, SourceLength - FinalLength);

    return (Rr_String){
        .Data = Buffer,
        .Length = FinalLength,
    };
}

Rr_String Rr_CreateEmptyString(size_t Length, Rr_Arena *Arena)
{
    return (Rr_String){
        .Data = RR_ALLOC(Arena, Length * sizeof(uint32_t)),
        .Length = Length,
    };
}

void Rr_UpdateString(Rr_String *String, size_t MaxLength, const char *CString, size_t LengthHint)
{
    size_t SourceLength = LengthHint > 0 ? LengthHint : strlen(CString);

    String->Length = Rr_UTF8ToUTF32(CString, LengthHint, String->Data, SourceLength);
}
