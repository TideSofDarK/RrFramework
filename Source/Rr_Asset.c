#include "Rr/Rr_Asset.h"

#if defined(RR_USE_RC)

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

Rr_Asset
Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    HRSRC Resource = FindResource(NULL, AssetRef, "RRDATA");
    HGLOBAL Memory = LoadResource(NULL, Resource);

    Rr_Asset Asset;
    Asset.Length = SizeofResource(NULL, Resource);
    Asset.Data = (Rr_Byte*)LockResource(Memory);
    return Asset;
}

#else

Rr_Asset
Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = {
        .Data = AssetRef.Start,
        .Length = (Rr_USize)(AssetRef.End - AssetRef.Start),
    };
    return Asset;
}

#endif
