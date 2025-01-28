#include <Rr/Rr_Asset.h>

#if defined(RR_USE_RC)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    HRSRC Resource = FindResource(NULL, AssetRef, "RRDATA");
    HGLOBAL Memory = LoadResource(NULL, Resource);

    Rr_Asset Asset;
    Asset.Length = SizeofResource(NULL, Resource);
    Asset.Data = (char *)LockResource(Memory);
    return Asset;
}

#else

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = {
        .Data = AssetRef.Start,
        .Size = (size_t)(AssetRef.End - AssetRef.Start),
    };
    return Asset;
}

#endif
