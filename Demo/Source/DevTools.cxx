#include "DevTools.hxx"

#include <cstdio>

#include <tinyexr/tinyexr.h>
#include <cstdlib>

void
HandleFileDrop(const char* Path)
{
    //    int Result;
    //    const char* Error;
    //
    //    EXRVersion Version;
    //    Result = ParseEXRVersionFromFile(&Version, Path);
    //    if (Result != 0)
    //    {
    //        abort();
    //    }
    //
    //    EXRHeader Header;
    //    Result = ParseEXRHeaderFromFile(&Header, &Version, Path, &Error);
    //    if (Result != 0)
    //    {
    //        FreeEXRErrorMessage(Error);
    //        abort();
    //    }
    //
    //    EXRImage Image;
    //    InitEXRImage(&Image);
    //
    //    Result = LoadEXRImageFromFile(&Image, &Header, Path, &Error);
    //    if (Result != 0)
    //    {
    //        FreeEXRHeader(&Header);
    //        FreeEXRErrorMessage(Error);
    //        abort();
    //    }
    //
    //    for (int Index = 0; Index < Image.width; Index++)
    //    {
    //        for (int Channel = 0; Channel < Header.num_channels; Channel++)
    //        {
    //            float* const Current = ((float*)Image.images[Channel]) + Index;
    //            fprintf(stderr, "%.10f      ", *Current);
    //        }
    //        fprintf(stderr, "\n");
    //    }
    //
    //    FreeEXRHeader(&Header);
    //    FreeEXRImage(&Image);
}
