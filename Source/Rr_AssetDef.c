#include "Rr_Asset.h"

#define Rr_DefineBuiltinAsset(NAME, PATH) INCBIN(NAME, STR(RR_BUILTIN_ASSET_PATH), PATH)

Rr_DefineBuiltinAsset(MartianMonoTTF, "MartianMono.ttf");
Rr_DefineBuiltinAsset(BuiltinFontPNG, "Iosevka.png");
Rr_DefineBuiltinAsset(BuiltinFontJSON, "Iosevka.json");

Rr_DefineBuiltinAsset(BuiltinTextVERT, "Text.vert.spv");
Rr_DefineBuiltinAsset(BuiltinTextFRAG, "Text.frag.spv");
