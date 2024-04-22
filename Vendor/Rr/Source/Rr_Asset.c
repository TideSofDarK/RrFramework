#include "Rr_Asset.h"

#define RrAsset_Define_Builtin(NAME, PATH) INCBIN(NAME, STR(RR_BUILTIN_ASSET_PATH), PATH)

RrAsset_Define_Builtin(MartianMonoTTF, "MartianMono.ttf");
RrAsset_Define_Builtin(NoisePNG, "Noise.png");