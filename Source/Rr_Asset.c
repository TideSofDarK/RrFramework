#include <Rr/Rr_Asset.h>

#if defined(RR_USE_RC)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    HRSRC Resource = FindResource(NULL, AssetRef, "RRDATA");
    HGLOBAL Memory = LoadResource(NULL, Resource);

    Rr_Asset Asset;
    Asset.Size = SizeofResource(NULL, Resource);
    Asset.Pointer = (char *)LockResource(Memory);
    return Asset;
}

#else

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = {
        .Size = (size_t)(AssetRef.End - AssetRef.Start),
        .Pointer = AssetRef.Start,
    };
    return Asset;
}

#endif
