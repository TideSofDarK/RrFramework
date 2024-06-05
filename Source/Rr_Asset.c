#include "Rr_Asset.h"

#if defined(RR_USE_RC)

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset;
    Asset.Resource = FindResource(NULL, AssetRef, "RRDATA");
    Asset.Memory = LoadResource(NULL, Asset.Resource);
    Asset.Length = SizeofResource(NULL, Asset.Resource);
    Asset.Data = (byte*)LockResource(Asset.Memory);
    return Asset;
}

#else

Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = {
        .Data = AssetRef.Start,
        .Length = (usize)(AssetRef.End - AssetRef.Start)
    };
    return Asset;
}

#endif
