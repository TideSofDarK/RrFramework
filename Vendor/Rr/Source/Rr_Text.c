#include "Rr_Text.h"

#include "Rr_Asset.h"

static void InitBuiltinFont(void)
{
    Rr_ExternAsset(BuiltinFontPNG);
    Rr_ExternAsset(BuiltinFontJSON);
}

void Rr_InitText(Rr_Renderer* Renderer)
{
    InitBuiltinFont();
}