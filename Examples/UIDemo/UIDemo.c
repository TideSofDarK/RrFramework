#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

static bool FixedSizeWindowOpen = false;
static bool ThemeEditorWindowOpen = false;
static bool TextInputWindowOpen = false;

static Rr_Image2D *VulkanImage = NULL;
static Rr_UIFont *StoneTombFont = NULL;
static Rr_UIFont *MozillaHeadlineFont = NULL;

static void TextInputWindow()
{
    Rr_UIBeginWindowEx("Text Input Window", &TextInputWindowOpen, 0);
    {
        static char StringBuffer[2048] = "";
        char LabelBuffer[64];
        snprintf(LabelBuffer, sizeof(LabelBuffer), "Length: %zu", strlen(StringBuffer));
        Rr_UILabelText("String Info", LabelBuffer);
        if (Rr_UIInputField(
                "String (2048 bytes)",
                sizeof(StringBuffer),
                StringBuffer,
                "Type here...",
                NULL,
                RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT))
        {
        }
    }
    Rr_UIEndWindow();
}

static void FixedSizeWindow()
{
    const Rr_Vec2 WINDOW_SIZE = Rr_V2(450.0f, 200.0f);
    Rr_UISetNextWindowExtent(WINDOW_SIZE);
    Rr_UIBeginWindowEx("Fixed Size Window", &FixedSizeWindowOpen, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT);
    {
        Rr_UIText("Resizing is disabled for this window.");
        Rr_UITextF("Window Size: %.0fx%.0f", WINDOW_SIZE.X, WINDOW_SIZE.Y);
    }
    Rr_UIEndWindow();
}

static void RandomizeColors(void)
{
    size_t ColorCount = sizeof(Rr_UIColors) / sizeof(Rr_Vec4);
    Rr_Vec4 *Colors = (Rr_Vec4 *)Rr_UIGetColors();
    for (size_t Index = 0; Index < ColorCount; ++Index)
    {
        Colors[Index].R = (float)rand() / (float)RAND_MAX;
        Colors[Index].G = (float)rand() / (float)RAND_MAX;
        Colors[Index].B = (float)rand() / (float)RAND_MAX;
    }
}

static inline void PrintStyle(const char *Name, float Style)
{
    fprintf(stdout, "Style->%s = %ff;\n", Name, Style);
}

static inline void PrintStyleVec2(const char *Name, Rr_Vec2 *Style)
{
    fprintf(stdout, "Style->%s = Rr_V2(%ff,%ff);\n", Name, Style->X, Style->Y);
}

static inline void PrintColor(const char *Name, Rr_Vec4 *Color)
{
    fprintf(stdout, "Colors->%s = Rr_V4(%ff,%ff,%ff,%ff);\n", Name, Color->R, Color->G, Color->B, Color->A);
}

static void PrintTheme(void)
{
    Rr_UIColors *Colors = Rr_UIGetColors();
    Rr_UIStyle *Style = Rr_UIGetStyle();

    fprintf(stdout, "\n/* RR UI THEME EXPORT BEGIN */\n");

    PrintStyle("FrameThickness", Style->FrameThickness);
    PrintStyleVec2("TitleBarPadding", &Style->TitleBarPadding);
    PrintStyleVec2("WindowPadding", &Style->WindowPadding);
    PrintStyleVec2("ContentsMargin", &Style->ContentsMargin);
    PrintStyle("ComponentMargin", Style->ComponentMargin);
    PrintStyle("ScrollbarAreaWidth", Style->ScrollbarAreaWidth);
    PrintStyle("BevelThickness", Style->BevelThickness);
    PrintStyle("DoubleBevelThickness", Style->TripleBevelThickness);
    PrintStyle("BevelIntensityLight", Style->BevelIntensityLight);
    PrintStyle("BevelIntensityDark", Style->BevelIntensityDark);
    PrintStyle("AlignedWidgetTitleMargin", Style->AlignedWidgetTitleMargin);
    PrintStyleVec2("ButtonPadding", &Style->ButtonPadding);
    PrintStyleVec2("InputFieldPadding", &Style->InputFieldPadding);
    PrintStyleVec2("CheckmarkRatios", &Style->CheckmarkRatios);
    PrintStyle("CheckmarkSize", Style->CheckmarkSize);
    PrintStyle("CrossWidth", Style->CrossWidth);
    PrintStyle("CrossThickness", Style->CrossThickness);

    PrintColor("Foreground", &Colors->Foreground);
    PrintColor("ForegroundDimmed", &Colors->ForegroundDimmed);
    PrintColor("Background", &Colors->Background);
    PrintColor("ChildBackground", &Colors->ChildBackground);
    PrintColor("ScrolloffBackground", &Colors->ScrolloffBackground);
    PrintColor("Outline", &Colors->Outline);
    PrintColor("SelectedOutline", &Colors->SelectedOutline);
    PrintColor("ListEntryBackgroundA", &Colors->ListEntryBackgroundA);
    PrintColor("ListEntryBackgroundB", &Colors->ListEntryBackgroundB);
    PrintColor("ListEntryHovered", &Colors->ListEntryHovered);

    PrintColor("TitleForeground", &Colors->TitleForeground);
    PrintColor("TitleBackground", &Colors->TitleBackground);
    PrintColor("TitleBackground2", &Colors->TitleBackground2);
    PrintColor("TitleBackgroundInactive", &Colors->TitleBackgroundInactive);
    PrintColor("TitleBackgroundTabs", &Colors->TitleBackgroundTabs);
    PrintColor("TitleCloseButtonBackground", &Colors->TitleCloseButtonBackground);
    PrintColor("TitleCollapseButtonBackground", &Colors->TitleCollapseButtonBackground);

    PrintColor("ScrollbarBackground", &Colors->ScrollbarBackground);
    PrintColor("ScrollbarNormal", &Colors->ScrollbarNormal);
    PrintColor("ScrollbarHovered", &Colors->ScrollbarHovered);
    PrintColor("ScrollbarHeld", &Colors->ScrollbarHeld);
    PrintColor("ResizeHandleNormal", &Colors->ResizeHandleNormal);
    PrintColor("ResizeHandleHovered", &Colors->ResizeHandleHovered);
    PrintColor("ResizeHandleHeld", &Colors->ResizeHandleHeld);

    PrintColor("ButtonNormal", &Colors->ButtonNormal);
    PrintColor("ButtonHovered", &Colors->ButtonHovered);
    PrintColor("ButtonHeld", &Colors->ButtonHeld);
    PrintColor("ButtonDisabled", &Colors->ButtonDisabled);

    PrintColor("ComboboxButtonNormal", &Colors->ComboboxButtonNormal);
    PrintColor("ComboboxButtonHeld", &Colors->ComboboxButtonHeld);
    PrintColor("ComboboxButtonActive", &Colors->ComboboxButtonActive);

    PrintColor("RadioButtonNormal", &Colors->RadioButtonNormal);
    PrintColor("RadioButtonOutline", &Colors->RadioButtonOutline);
    PrintColor("RadioButtonHeld", &Colors->RadioButtonHeld);

    PrintColor("InputFieldNormal", &Colors->InputFieldNormal);
    PrintColor("InputFieldActive", &Colors->InputFieldActive);
    PrintColor("SelectedTextBackground", &Colors->SelectedTextBackground);
    PrintColor("SelectedTextForeground", &Colors->SelectedTextForeground);

    fprintf(stdout, "/* RR UI THEME EXPORT END */\n\n");
}

static void ThemeEditorWindow()
{
    Rr_UIBeginWindowEx("Theme Editor", &ThemeEditorWindowOpen, 0);
    {
        Rr_UIStyle *Style = Rr_UIGetStyle();
        Rr_UIColors *Colors = Rr_UIGetColors();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloatRange("Frame Thickness", &Style->FrameThickness, 0.0f, 0.125f);
        Rr_UIInputFloat2ZO("Title Padding", Style->TitleBarPadding.Elements);
        Rr_UIInputFloat2ZO("Window Padding", Style->WindowPadding.Elements);
        Rr_UIInputFloat2ZO("Contents Margin", Style->ContentsMargin.Elements);
        Rr_UIInputFloatZO("Component Margin", &Style->ComponentMargin);
        Rr_UIInputFloatZO("Bevel Thickness", &Style->BevelThickness);
        Rr_UIInputFloatZO("Double Bevel Thickness", &Style->TripleBevelThickness);
        Rr_UIInputFloatZO("Bevel Intensity Light", &Style->BevelIntensityLight);
        Rr_UIInputFloatZO("Bevel Intensity Dark", &Style->BevelIntensityDark);
        Rr_UIInputFloatZO("Flexible Title Margin", &Style->AlignedWidgetTitleMargin);
        Rr_UIInputFloat2ZO("Checkmark Ratios", Style->CheckmarkRatios.Elements);
        Rr_UIInputFloatZO("Checkmark Size", &Style->CheckmarkSize);
        Rr_UIInputFloatZO("Cross Width", &Style->CrossWidth);
        Rr_UIInputFloatZO("Cross Thickness", &Style->CrossThickness);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UISeparator();

        Rr_UIInputColor4("Foreground", Colors->Foreground.Elements);
        Rr_UIInputColor4("Foreground Dimmed", Colors->ForegroundDimmed.Elements);
        Rr_UIInputColor4("Background", Colors->Background.Elements);
        Rr_UIInputColor4("Child Background", Colors->ChildBackground.Elements);
        Rr_UIInputColor4("Scrolloff Background", Colors->ScrolloffBackground.Elements);
        Rr_UIInputColor4("Outline", Colors->Outline.Elements);
        Rr_UIInputColor4("Selected Outline", Colors->SelectedOutline.Elements);
        Rr_UIInputColor4("List Entry Background A", Colors->ListEntryBackgroundA.Elements);
        Rr_UIInputColor4("List Entry Background B", Colors->ListEntryBackgroundB.Elements);
        Rr_UIInputColor4("List Entry Hovered", Colors->ListEntryHovered.Elements);

        Rr_UISeparator();

        Rr_UIInputColor4("Title Foreground", Colors->TitleForeground.Elements);
        Rr_UIInputColor4("Title Background", Colors->TitleBackground.Elements);
        Rr_UIInputColor4("Title Background 2", Colors->TitleBackground2.Elements);
        Rr_UIInputColor4("Title Background Inactive", Colors->TitleBackgroundInactive.Elements);
        Rr_UIInputColor4("Title Background Tabs", Colors->TitleBackgroundTabs.Elements);
        Rr_UIInputColor4("Title Close Button", Colors->TitleCloseButtonBackground.Elements);
        Rr_UIInputColor4("Title Collapse Button", Colors->TitleCollapseButtonBackground.Elements);

        Rr_UISeparator();

        Rr_UIInputFloatRange("Scrollbar Area Width", &Style->ScrollbarAreaWidth, 0.001f, 2.0f);
        Rr_UIInputColor4("Scrollbar Background", Colors->ScrollbarBackground.Elements);
        Rr_UIInputColor4("Scrollbar Normal", Colors->ScrollbarNormal.Elements);
        /* Rr_UIInputColor4( */
        /*     "Scrollbar Hovered", */
        /*     Colors->ScrollbarHovered.Elements); */
        Rr_UIInputColor4("Scrollbar Held", Colors->ScrollbarHeld.Elements);
        Rr_UIInputColor4("Resize Normal", Colors->ResizeHandleNormal.Elements);
        Rr_UIInputColor4("Resize Hovered", Colors->ResizeHandleHovered.Elements);
        Rr_UIInputColor4("Resize Held", Colors->ResizeHandleHeld.Elements);

        Rr_UISeparator();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2ZO("Button Padding", Style->ButtonPadding.Elements);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UIInputColor4("Button Normal", Colors->ButtonNormal.Elements);
        /* Rr_UIInputColor4("Button Hovered", Colors->ButtonHovered.Elements);
         */
        Rr_UIInputColor4("Button Held", Colors->ButtonHeld.Elements);
        /* Rr_UIInputColor4("Button Disabled", Colors->ButtonDisabled.Elements);
         */

        Rr_UIInputColor4("Combobox Normal", Colors->ComboboxButtonNormal.Elements);
        Rr_UIInputColor4("Combobox Held", Colors->ComboboxButtonHeld.Elements);
        Rr_UIInputColor4("Combobox Active", Colors->ComboboxButtonActive.Elements);

        Rr_UIInputColor4("Radio Normal", Colors->RadioButtonNormal.Elements);
        Rr_UIInputColor4("Radio Outline", Colors->RadioButtonOutline.Elements);
        Rr_UIInputColor4("Radio Held", Colors->RadioButtonHeld.Elements);

        Rr_UISeparator();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2ZO("Input Field Padding", Style->InputFieldPadding.Elements);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UIInputColor4("Input Field Normal", Colors->InputFieldNormal.Elements);
        Rr_UIInputColor4("Input Field Active", Colors->InputFieldActive.Elements);
        Rr_UIInputColor4("Selected Text BG", Colors->SelectedTextBackground.Elements);
        Rr_UIInputColor4("Selected Text FG", Colors->SelectedTextForeground.Elements);

        Rr_UISeparator();

        Rr_UIBeginHorizontal();
        if (Rr_UIButton("Print Theme"))
        {
            PrintTheme();
        }
        if (Rr_UIButton("Randomize Colors"))
        {
            RandomizeColors();
        }
        Rr_UIEndHorizontal();
    }
    Rr_UIEndWindow();
}

static void SetOliveTheme()
{
    Rr_UIColors *Colors = Rr_UIGetColors();
    Rr_UIStyle *Style = Rr_UIGetStyle();

    Style->FrameThickness = 0.026000f;
    Style->TitleBarPadding = Rr_V2(0.250000f, 0.025000f);
    Style->WindowPadding = Rr_V2(0.500000f, 0.500000f);
    Style->ContentsMargin = Rr_V2(0.250000f, 0.250000f);
    Style->ComponentMargin = 0.200000f;
    Style->ScrollbarAreaWidth = 0.750000f;
    Style->BevelThickness = 0.015000f;
    Style->BevelIntensityLight = 0.350000f;
    Style->BevelIntensityDark = 0.200000f;
    Style->AlignedWidgetTitleMargin = 0.500000f;
    Style->ButtonPadding = Rr_V2(0.250000f, 0.025000f);
    Style->InputFieldPadding = Rr_V2(0.250000f, 0.025000f);
    Style->CheckmarkRatios = Rr_V2(0.35000f, 0.200000f);
    Style->CheckmarkSize = 0.75000f;
    Style->CrossWidth = 0.650000f;
    Style->CrossThickness = 0.035000f;
    Colors->Foreground = Rr_V4(0.000000f, 0.000000f, 0.000000f, 1.000000f);
    Colors->ForegroundDimmed = Rr_V4(0.199287f, 0.199287f, 0.199287f, 1.000000f);
    Colors->Background = Rr_V4(0.666667f, 0.666667f, 0.666667f, 1.000000f);
    Colors->ChildBackground = Rr_V4(0.752941f, 0.752941f, 0.752941f, 1.000000f);
    Colors->ScrolloffBackground = Rr_V4(0.666667f, 0.666667f, 0.666667f, 1.000000f);
    Colors->Outline = Rr_V4(0.627451f, 0.627451f, 0.627451f, 1.000000f);
    Colors->SelectedOutline = Rr_V4(0.564706f, 0.592157f, 0.521569f, 1.000000f);
    Colors->ListEntryBackgroundA = Rr_V4(0.752941f, 0.752941f, 0.752941f, 1.000000f);
    Colors->ListEntryBackgroundB = Rr_V4(0.727996f, 0.727996f, 0.727996f, 1.000000f);
    Colors->ListEntryHovered = Rr_V4(0.776471f, 0.854902f, 0.486275f, 1.000000f);
    Colors->TitleForeground = Rr_V4(0.898039f, 0.921569f, 0.929412f, 1.000000f);
    Colors->TitleBackground = Rr_V4(0.568627f, 0.619608f, 0.376471f, 1.000000f);
    Colors->TitleBackground2 = Rr_V4(0.388235f, 0.431373f, 0.231373f, 1.000000f);
    Colors->TitleCloseButtonBackground = Rr_V4(0.601035f, 0.141315f, 0.190579f, 1.000000f);
    Colors->TitleCollapseButtonBackground = Rr_V4(0.568627f, 0.619608f, 0.376471f, 1.000000f);
    Colors->ScrollbarBackground = Rr_V4(0.576471f, 0.576471f, 0.576471f, 1.000000f);
    Colors->ScrollbarNormal = Rr_V4(0.474510f, 0.505882f, 0.368627f, 1.000000f);
    Colors->ScrollbarHovered = Rr_V4(0.670588f, 0.698039f, 0.611765f, 1.000000f);
    Colors->ScrollbarHeld = Rr_V4(0.568627f, 0.619608f, 0.376471f, 1.000000f);
    Colors->ResizeHandleNormal = Rr_V4(0.423529f, 0.423529f, 0.423529f, 1.000000f);
    Colors->ResizeHandleHovered = Rr_V4(0.474510f, 0.505882f, 0.368627f, 1.000000f);
    Colors->ResizeHandleHeld = Rr_V4(0.568627f, 0.619608f, 0.376471f, 1.000000f);
    Colors->ButtonNormal = Rr_V4(0.777635f, 0.777635f, 0.777635f, 1.000000f);
    Colors->ButtonHovered = Rr_V4(0.781696f, 0.781696f, 0.781696f, 1.000000f);
    Colors->ButtonHeld = Rr_V4(0.686275f, 0.686275f, 0.686275f, 1.000000f);
    Colors->ButtonDisabled = Rr_V4(0.070520f, 0.093346f, 0.111383f, 1.000000f);
    Colors->ComboboxButtonNormal = Rr_V4(0.666667f, 0.666667f, 0.666667f, 1.000000f);
    Colors->ComboboxButtonHeld = Rr_V4(0.576471f, 0.576471f, 0.576471f, 1.000000f);
    Colors->ComboboxButtonActive = Rr_V4(0.564706f, 0.592157f, 0.521569f, 1.000000f);
    Colors->RadioButtonNormal = Rr_V4(0.686275f, 0.686275f, 0.686275f, 1.000000f);
    Colors->RadioButtonOutline = Rr_V4(0.576471f, 0.576471f, 0.576471f, 1.000000f);
    Colors->RadioButtonHeld = Rr_V4(0.568627f, 0.619608f, 0.376471f, 1.000000f);
    Colors->InputFieldNormal = Rr_V4(0.568627f, 0.568627f, 0.568627f, 1.000000f);
    Colors->InputFieldActive = Rr_V4(0.564706f, 0.592157f, 0.521569f, 1.000000f);
    Colors->SelectedTextBackground = Rr_V4(0.776471f, 0.854902f, 0.486275f, 1.000000f);
    Colors->SelectedTextForeground = Rr_V4(0.030000f, 0.030000f, 0.030000f, 1.000000f);
}

static void SetPinkTheme()
{
    Rr_UIColors *Colors = Rr_UIGetColors();
    Rr_UIStyle *Style = Rr_UIGetStyle();

    Style->FrameThickness = 0.075000f;
    Style->TitleBarPadding = Rr_V2(0.250000f, 0.025000f);
    Style->WindowPadding = Rr_V2(0.300000f, 0.300000f);
    Style->ContentsMargin = Rr_V2(0.250000f, 0.250000f);
    Style->ComponentMargin = 0.200000f;
    Style->ScrollbarAreaWidth = 0.750000f;
    Style->BevelThickness = 0.050000f;
    Style->TripleBevelThickness = 0.100000f;
    Style->BevelIntensityLight = 0.300000f;
    Style->BevelIntensityDark = 0.650000f;
    Style->AlignedWidgetTitleMargin = 0.250000f;
    Style->ButtonPadding = Rr_V2(0.250000f, 0.025000f);
    Style->InputFieldPadding = Rr_V2(0.250000f, 0.025000f);
    Style->CheckmarkRatios = Rr_V2(0.350000f, 0.200000f);
    Style->CheckmarkSize = 0.750000f;
    Style->CrossWidth = 0.650000f;
    Style->CrossThickness = 0.135000f;
    Colors->Foreground = Rr_V4(0.940547f, 0.899630f, 0.970019f, 1.000000f);
    Colors->ForegroundDimmed = Rr_V4(0.634900f, 0.617390f, 0.660727f, 1.000000f);
    Colors->Background = Rr_V4(0.153051f, 0.142532f, 0.186284f, 1.000000f);
    Colors->ChildBackground = Rr_V4(0.152941f, 0.141176f, 0.184314f, 1.000000f);
    Colors->ScrolloffBackground = Rr_V4(0.152941f, 0.141176f, 0.184314f, 1.000000f);
    Colors->Outline = Rr_V4(0.152941f, 0.141176f, 0.184314f, 1.000000f);
    Colors->SelectedOutline = Rr_V4(0.314730f, 0.286137f, 0.390978f, 1.000000f);
    Colors->ListEntryBackgroundA = Rr_V4(0.334322f, 0.277384f, 0.413703f, 1.000000f);
    Colors->ListEntryBackgroundB = Rr_V4(0.284174f, 0.235776f, 0.351648f, 1.000000f);
    Colors->ListEntryHovered = Rr_V4(0.463433f, 0.343573f, 0.643222f, 1.000000f);
    Colors->TitleForeground = Rr_V4(0.937255f, 0.898039f, 0.968627f, 1.000000f);
    Colors->TitleBackground = Rr_V4(0.472738f, 0.325686f, 0.538689f, 1.000000f);
    Colors->TitleBackground2 = Rr_V4(0.203582f, 0.122154f, 0.240995f, 1.000000f);
    Colors->TitleBackgroundInactive = Rr_V4(0.334322f, 0.277384f, 0.413703f, 1.000000f);
    Colors->TitleCloseButtonBackground = Rr_V4(0.890921f, 0.129675f, 0.182090f, 1.000000f);
    Colors->TitleCollapseButtonBackground = Rr_V4(0.470588f, 0.325490f, 0.537255f, 1.000000f);
    Colors->ScrollbarBackground = Rr_V4(0.070520f, 0.093346f, 0.111383f, 1.000000f);
    Colors->ScrollbarNormal = Rr_V4(0.322105f, 0.261189f, 0.365981f, 1.000000f);
    Colors->ScrollbarHovered = Rr_V4(0.408665f, 0.497836f, 0.557879f, 1.000000f);
    Colors->ScrollbarHeld = Rr_V4(0.321569f, 0.258824f, 0.364706f, 1.000000f);
    Colors->ResizeHandleNormal = Rr_V4(0.937255f, 0.898039f, 0.968627f, 1.000000f);
    Colors->ResizeHandleHovered = Rr_V4(0.717092f, 0.636152f, 0.781843f, 1.000000f);
    Colors->ResizeHandleHeld = Rr_V4(0.713726f, 0.635294f, 0.780392f, 1.000000f);
    Colors->ButtonNormal = Rr_V4(0.334322f, 0.277384f, 0.413703f, 1.000000f);
    Colors->ButtonHovered = Rr_V4(0.408665f, 0.497836f, 0.557879f, 1.000000f);
    Colors->ButtonHeld = Rr_V4(0.463433f, 0.343573f, 0.643222f, 1.000000f);
    Colors->ButtonDisabled = Rr_V4(0.070520f, 0.093346f, 0.111383f, 1.000000f);
    Colors->ComboboxButtonNormal = Rr_V4(0.196078f, 0.133333f, 0.231373f, 1.000000f);
    Colors->ComboboxButtonHeld = Rr_V4(0.462745f, 0.341176f, 0.643137f, 1.000000f);
    Colors->ComboboxButtonActive = Rr_V4(0.333333f, 0.192157f, 0.411765f, 1.000000f);
    Colors->RadioButtonNormal = Rr_V4(0.333333f, 0.274510f, 0.411765f, 1.000000f);
    Colors->RadioButtonOutline = Rr_V4(0.462745f, 0.341176f, 0.643137f, 1.000000f);
    Colors->RadioButtonHeld = Rr_V4(0.462745f, 0.341176f, 0.643137f, 1.000000f);
    Colors->InputFieldNormal = Rr_V4(0.199295f, 0.134664f, 0.234178f, 1.000000f);
    Colors->InputFieldActive = Rr_V4(0.334506f, 0.193713f, 0.413703f, 1.000000f);
    Colors->SelectedTextBackground = Rr_V4(0.793058f, 0.188728f, 1.000000f, 1.000000f);
    Colors->SelectedTextForeground = Rr_V4(1.000000f, 1.000000f, 1.000000f, 1.000000f);
}

static Rr_Image2D *CreateColorImageFromPNG(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);
    int32_t DesiredChannels = 4;
    int32_t Width, Height, Channels;
    char *Data = (char *)
        stbi_load_from_memory((stbi_uc *)Asset.Data, (int32_t)Asset.Size, &Width, &Height, &Channels, DesiredChannels);

    Rr_Image2D *ColorImage = Rr_CreateImage2D(
        (Rr_IntVec2){ Width, Height },
        RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    size_t Size = Width * Height * DesiredChannels;

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(Size, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);

    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, (Rr_IntVec2){ Width, Height }, ColorImage, 0);

    Rr_ReleaseBuffer(StagingBuffer);

    return ColorImage;
}

static void Init(void)
{
    VulkanImage = CreateColorImageFromPNG(EXAMPLE_ASSET_VULKAN_PNG);

    Rr_Asset StoneTombAsset = Rr_LoadAsset(EXAMPLE_ASSET_STONETOMB_TTF);
    StoneTombFont = Rr_UICreateFont(StoneTombAsset.Size, StoneTombAsset.Data, 18.0f);

    Rr_Asset MozillaHeadlineCleanAsset = Rr_LoadAsset(EXAMPLE_ASSET_MOZILLAHEADLINE_TTF);
    MozillaHeadlineFont = Rr_UICreateFont(MozillaHeadlineCleanAsset.Size, MozillaHeadlineCleanAsset.Data, 14.0f);
}

static void GeneralWindow(void)
{
    static bool GeneralWindowOpen = true;
    Rr_UIBeginWindowEx("Rr_UI.h - General", &GeneralWindowOpen, 0);
    {
        Rr_UISetNextWindowCreateCollapsed(false);
        Rr_UIBeginWindow("Style and Colors");
        {
            Rr_UIText("Colors set in Theme Editor can be printed to stdout.");

            if (Rr_UIButton("Show Theme Editor"))
            {
                ThemeEditorWindowOpen = true;
            }

            if (Rr_UIButton("Set Default Theme"))
            {
                Rr_UISetDefaultTheme();
            }

            if (Rr_UIButton("Set Olive Theme"))
            {
                SetOliveTheme();
            }

            if (Rr_UIButton("Set Pink Theme"))
            {
                SetPinkTheme();
            }
        }
        Rr_UIEndWindow();

        Rr_UISetNextWindowCreateCollapsed(false);
        Rr_UIBeginWindow("Fonts");
        {
            Rr_UITextWrapped(
                "Fonts can be dynamically pushed onto the stack. All paddings "
                "and margins are dependent on current fonts line height but "
                "could be overriden with absolute values.");

            Rr_UIAdvance((Rr_Vec2){ 0.0f, Rr_UICurrentLineHeight() * 0.25f });

            Rr_UIPushFont(MozillaHeadlineFont);

            Rr_UIText("Switching to Mozilla Headline now.");

            Rr_UIButton("Button With This Font");

            static Rr_Vec3 ColorRGB = { 0.2f, 0.3f, 0.4f };
            Rr_UIInputColor3("Color RGB", ColorRGB.Elements);

            Rr_UIPopFont();

            Rr_UIAdvance((Rr_Vec2){ 0.0f, Rr_UICurrentLineHeight() * 0.25f });

            Rr_UIPushFont(StoneTombFont);

            Rr_UIText("Different font, boo!");

            static float ScaryFloat = 0.45f;
            Rr_UISliderFloat("Scary Slider", &ScaryFloat, -1024.0f, 1024.0f);

            Rr_UIBeginHorizontal();
            static int Radio = -1;
            Rr_UIRadioButton("Radio A", &Radio, 0);
            Rr_UIRadioButton("Radio B", &Radio, 1);
            Rr_UIRadioButton("Radio C", &Radio, 2);
            Rr_UIEndHorizontal();

            Rr_UIPopFont();
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Images");
        {
            Rr_UIImageEx(VulkanImage, Rr_V2(600.0f, 400.0f), Rr_V2F(0.0f), Rr_V2F(1.0f));
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Child Windows");
        {
            Rr_UIText(
                "This is an example of a child window.\nUse child windows to "
                "group your widgets.");

            Rr_UIBeginWindowEx("Undockable Child Window", NULL, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT);
            {
                Rr_UIText("Hold CTRL and click its title to undock this window.");
            }
            Rr_UIEndWindow();

            Rr_UIBeginWindowEx("Custom Child Window", NULL, RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT);
            {
                Rr_UIText("This child window has no title bar.");
            }
            Rr_UIEndWindow();
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Tabs");
        {
            Rr_UIBeginWindowEx("Example Tabs", NULL, RR_UI_WINDOW_FLAGS_TABS_BIT);
            {
                Rr_UIBeginWindow("Text");
                {
                    Rr_UIText("Some text...");
                    Rr_UIText("Some text...");
                    Rr_UIText("Some text...");
                }
                Rr_UIEndWindow();

                Rr_UIBeginWindow("Buttons");
                {
                    Rr_UIButton("Button A");
                    Rr_UIButton("Button B");
                    Rr_UIButton("Button C");
                }
                Rr_UIEndWindow();

                Rr_UIBeginWindow("Input Fields");
                {
                    static Rr_Vec2 TestVec2 = { -0.5f, 0.5f };
                    Rr_UIInputFloat2NO("Float2 Input", TestVec2.Elements);
                    static Rr_Vec3 TestVec3 = { -0.5f, 0.5f };
                    Rr_UIInputFloat3NO("Float3 Input", TestVec3.Elements);
                    static Rr_Vec4 TestVec4 = { -0.5f, 0.5f };
                    Rr_UIInputFloat4NO("Float4 Input", TestVec4.Elements);
                }
                Rr_UIEndWindow();
            }
            Rr_UIEndWindow();
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Trees");
        {
            Rr_UIBeginHorizontal();
            if (Rr_UIButton("Expand All"))
            {
                Rr_UISetNextTreeExpanded();
            }
            if (Rr_UIButton("Collapse All"))
            {
                Rr_UISetNextTreeCollapsed();
            }
            Rr_UIEndHorizontal();

            if (Rr_UIBeginTree("Tree #1"))
            {
                static Rr_Vec2 Vec2 = { -75.0f, 125.0f };
                Rr_UIInputFloat2("Float2 Input", Vec2.Elements);

                if (Rr_UIBeginTree("Tree #2###0"))
                {
                    static int Count = 0;
                    if (Rr_UIButton("Click me!"))
                    {
                        Count++;
                    }
                    if (Count)
                    {
                        Rr_UIText("Thanks for clicking me!");
                    }

                    if (Rr_UIBeginTree("Tree #3"))
                    {
                        static int Radio = -1;
                        Rr_UIRadioButton("Radio A", &Radio, 0);
                        Rr_UIRadioButton("Radio B", &Radio, 1);
                        Rr_UIRadioButton("Radio C", &Radio, 2);

                        Rr_UIEndTree();
                    }

                    Rr_UIEndTree();
                }

                if (Rr_UIBeginTree("Tree #2###1"))
                {
                    static bool Checked = false;
                    Rr_UICheckbox("Checkbox", &Checked);

                    Rr_UIEndTree();
                }

                Rr_UIEndTree();
            }
        }
        Rr_UIEndWindow();

        if (Rr_UIBeginWindow("Custom Draw"))
        {
            Rr_Vec2 Cursor = Rr_UIGetCursor();

            const Rr_Vec2 AREA = Rr_V2(300.0f, 200.0f);

            Rr_UIDrawCircleFilled(
                Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.25f, AREA.Y * 0.5f)),
                AREA.X * 0.25f,
                &Rr_UIGetColors()->Foreground);

            Rr_UIDrawEquilateralTriangleFilled(
                Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.75f, AREA.Y - AREA.X * 0.4f * 0.5f)),
                AREA.X * 0.4f,
                RR_ANGLE_DEG(-90.0f),
                &Rr_UIGetColors()->Foreground);

            float PhaseA = 1.0f + cosf(Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            float PhaseB = 1.0f + cosf(RR_PI * 0.25f + Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            float PhaseC = 1.0f + cosf(RR_PI * 0.5f + Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            float PhaseD = 1.0f + cosf(RR_PI * 0.75f + Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            Rr_UIVertex QuadVertices[4] = {
                {
                    .Position = Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.25f * PhaseA, AREA.Y * 0.25f * PhaseB)),
                    .Color = Rr_V4(1.0f, 0.0f, 0.0f, 1.0f),
                },
                {
                    .Position = Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.75f * PhaseB, AREA.Y * 0.25f * PhaseA)),
                    .Color = Rr_V4(0.0f, 1.0f, 0.0f, 1.0f),
                },
                {
                    .Position = Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.75f * PhaseC, AREA.Y * 0.75f * PhaseD)),
                    .Color = Rr_V4(0.0f, 1.0f, 1.0f, 1.0f),
                },
                {
                    .Position = Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.25f + PhaseD, AREA.Y * 0.75f * PhaseC)),
                    .Color = Rr_V4(1.0f, 0.0f, 1.0f, 1.0f),
                },
            };
            Rr_UIDrawQuadVertices(QuadVertices);

            Rr_UIAdvance(AREA);
        }
        Rr_UIEndWindow();
    }
    Rr_UIEndWindow();
}

static void WidgetsWindow(void)
{
    static bool NoClose = false;
    static bool NoResize = false;
    static bool NoHorizontalScrollbar = false;
    static bool NoVerticalScrollbar = false;
    static bool NoTitleBar = false;
    static bool NoBorders = false;
    static bool AutoResize = true;
    static bool NoBackground = false;

    Rr_UIWindowFlags Flags = 0;
    if (NoClose)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_CLOSE_BIT;
    }
    if (NoResize)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT;
    }
    if (NoHorizontalScrollbar)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_HORIZONTAL_SCROLLBAR_BIT;
    }
    if (NoVerticalScrollbar)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT;
    }
    if (NoTitleBar)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT;
    }
    if (NoBorders)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT;
    }
    if (AutoResize)
    {
        Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;
    }
    if (NoBackground)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_BACKGROUND_BIT;
    }

    static bool WidgetsWindowOpen = true;
    Rr_UIBeginWindowEx("Rr_UI.h - Widgets", &WidgetsWindowOpen, Flags);
    {
        Rr_UISetNextWindowCreateCollapsed(false);
        Rr_UIBeginWindow("Text");
        {
            Rr_UIText("Simple Text");
            Rr_UIText(
                " - Leading Spaces and Line Breaks 1\n - Leading Spaces and "
                "Line Breaks 2");
            Rr_UILabelText("Label", "* Line 1\n* Line 2\n* Line 3");
            Rr_UITextWrapped(
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. "
                "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                "laboris nisi ut aliquip ex ea commodo consequat.");
        }
        Rr_UIEndWindow();

        Rr_UISetNextWindowCreateCollapsed(false);
        Rr_UIBeginWindow("Buttons");
        {
            if (Rr_UIButton("Show Fixed Size Window"))
            {
                FixedSizeWindowOpen = true;
            }

            if (Rr_UIButton("Show Text Input Window"))
            {
                TextInputWindowOpen = true;
            }

            Rr_UIBeginHorizontal();
            Rr_UIPushContentsMargin(Rr_V2F(0.0f));
            Rr_UIPushWidgetExtent(Rr_V2F(Rr_UICurrentLineHeight() * 1.25f));
            Rr_UIButton("X");
            Rr_UIButton("Y");
            Rr_UIButton("Z");
            Rr_UIEndHorizontal();
            Rr_UIBeginHorizontal();
            Rr_UIButton("U");
            Rr_UIButton("V");
            Rr_UIButton("W");
            Rr_UIPopWidgetExtent();
            Rr_UIPopContentsMargin();
            Rr_UIEndHorizontal();

            Rr_UIBeginHorizontal();
            static int32_t SelectedRadioButton = 0;
            Rr_UIRadioButton("Radio A", &SelectedRadioButton, 0);
            Rr_UIRadioButton("Radio B", &SelectedRadioButton, 1);
            Rr_UIRadioButton("Radio C", &SelectedRadioButton, 2);
            Rr_UIEndHorizontal();
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Comboboxes");
        {
            const char *ComboboxOptions[5] = {
                "Option A", "Option B", "Option C", "Option D", "Longer Option E",
            };
            static uint32_t SelectedComboboxOption = 0;
            if (Rr_UICombobox("Options", RR_ARRAY_COUNT(ComboboxOptions), ComboboxOptions, &SelectedComboboxOption))
            {
                fprintf(stderr, "New option selected: %s\n", ComboboxOptions[SelectedComboboxOption]);
            }
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Checkboxes");
        {
            Rr_UICheckbox("Close Button", &NoClose);
            Rr_UICheckbox("No Resize", &NoResize);
            Rr_UICheckbox("Auto Resize", &AutoResize);
            Rr_UICheckbox("No Horizontal Scrollbar", &NoHorizontalScrollbar);
            Rr_UICheckbox("No Vertical Scrollbar", &NoVerticalScrollbar);
            Rr_UICheckbox("No Title Bar", &NoTitleBar);
            Rr_UICheckbox("No Borders", &NoBorders);
            Rr_UICheckbox("No Background", &NoBackground);
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Sliders");
        {
            static float FloatA = 0.45f;
            Rr_UISliderFloat("Float 0 to 1", &FloatA, 0.0f, 1.0f);
            static float FloatB = -16.0f;
            Rr_UISliderFloat("Float -32 to 32", &FloatB, -32.0f, 32.0f);
            static int32_t Int = 0;
            Rr_UISliderInt("Int -2 to 2", &Int, -2, 2);
            static uint32_t UnsignedInt = 0;
            Rr_UISliderUnsignedInt("Unsigned 2 to 8", &UnsignedInt, 2, 8);
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindow("Colors");
        {
            static Rr_Vec3 ColorRGB = { 0.2f, 0.3f, 0.4f };
            Rr_UIInputColor3("Color RGB", ColorRGB.Elements);
            static Rr_Vec4 ColorRGBA = { 0.9f, 0.2345f, 0.2f, 0.5f };
            Rr_UIInputColor4("Color RGBA", ColorRGBA.Elements);
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindowEx("Input Fields", NULL, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT);
        {
            static char StringBuffer[16] = "Hello, World!";
            Rr_UIInputText("String (16 bytes)", 16, StringBuffer);

            static char MultilineBuffer[128] = "Line A\nLine B <- Delete this!\nLine C!";
            Rr_UIInputText("String (128 bytes)", 128, MultilineBuffer);

            static int32_t TestInt = 1337;
            Rr_UIInputInt("Integer Input", &TestInt);

            static int32_t TestIntRange = 5;
            Rr_UIInputIntRange("Integer Input (2-8)", &TestIntRange, 2, 8);

            static uint32_t TestUnsignedInt = 348579;
            Rr_UIInputUnsignedInt("Unsigned Input", &TestUnsignedInt);

            static uint32_t TestUnsignedIntRange = 6;
            Rr_UIInputUnsignedIntRange("Unsigned Input (3-9)", &TestUnsignedIntRange, 3, 9);

            static float TestFloat = 1234.567f;
            Rr_UIInputFloat("Float Input", &TestFloat);

            static float TestFloatRange = 0.25f;
            Rr_UIInputFloatRange("Float Input (0-1)", &TestFloatRange, 0.0f, 1.0f);

            static double TestDouble = -3233463426234234.125125;
            Rr_UIInputDouble("Double Input", &TestDouble);

            Rr_UIBeginWindow("Vectors and Matrices");
            {
                static Rr_Vec2 TestVec2 = { 1.0f, 0.0f };
                Rr_UIInputFloat2("Float2 Input", TestVec2.Elements);
                static Rr_Vec3 TestVec3 = { 1.0f, 0.0f, 1.0f };
                Rr_UIInputFloat3("Float3 Input", TestVec3.Elements);
                static Rr_Vec4 TestVec4 = { 1.0f, 0.0f, 1.0f, 1.0f };
                Rr_UIInputFloat4("Float4 Input", TestVec4.Elements);
                static Rr_Mat2 TestMat2 = {
                    1.0f,
                    -1.0f, //
                    -1.0f,
                    1.0f, //
                };
                Rr_UIInputFloat2x2("Float2x2 Input", (float *)TestMat2.Elements);
                static Rr_Mat3 TestMat3 = {
                    1.0f,  -1.0f, 1.0f,  //
                    -1.0f, 1.0f,  -1.0f, //
                    1.0f,  -1.0f, 1.0f,  //
                };
                Rr_UIInputFloat3x3("Float3x3 Input", (float *)TestMat3.Elements);
                static Rr_Mat4 TestMat4 = {
                    1.0f,  -1.0f, 1.0f,  -1.0f, //
                    -1.0f, 1.0f,  -1.0f, 1.0f,  //
                    1.0f,  -1.0f, 1.0f,  -1.0f, //
                    -1.0f, 1.0f,  -1.0f, 1.0f,  //
                };
                Rr_UIInputFloat4x4("Float4x4 Input", (float *)TestMat4.Elements);
            }
            Rr_UIEndWindow();
        }
        Rr_UIEndWindow();
    }
    Rr_UIEndWindow();
}

static void TestWindow(void)
{
    Rr_UIWindowFlags Flags = 0;
    /* Flags |= RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT; */
    /* Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT; */
    if (Rr_UIBeginWindowEx("Teddd", NULL, Flags))
    {
        static bool Bools[4] = { 0 };
        Rr_UICheckbox("Checkbox", &Bools[0]);
        /* if (Bools[0]) */
        {
            Rr_UICheckbox("Checkbox ==", &Bools[1]);
        }
        /* if (Bools[1]) */
        {
            Rr_UICheckbox("Checkbox ======", &Bools[2]);
        }
        Rr_UITextWrapped(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
            "do eiusmod tempor incididunt ut labore et dolore magna "
            "aliqua. "
            "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
            "laboris nisi ut aliquip ex ea commodo consequat.");
        /* if (Bools[2]) */
        {
            Rr_UICheckbox("Checkbox =========", &Bools[3]);
        }
    }
    Rr_UIEndWindow();
}

static void Iterate(void)
{
    Rr_UIDebugOverlay();

    Rr_Graph *Graph = Rr_GetGraph();

    Rr_ClearColorImage2D(Graph, (Rr_ColorClear){ 0.01f, 0.01f, 0.02f, 1.0f }, Rr_GetSwapchainImage());

    FixedSizeWindow();
    ThemeEditorWindow();
    TextInputWindow();
    GeneralWindow();
    WidgetsWindow();
    /* TestWindow(); */
}

static void Event(Rr_Event const *Event)
{
    if (Event->Type == RR_EVENT_TYPE_KEY_DOWN)
    {
        static bool SlowMo = false;
        if (Event->Key.Scancode == RR_SCANCODE_F5)
        {
            SlowMo = !SlowMo;
            Rr_SetTargetFrameRate(SlowMo ? 2 : 180);
        }
    }
}

static void Cleanup(void)
{
    Rr_ReleaseImage(VulkanImage);
    Rr_UIReleaseFont(StoneTombFont);
    Rr_UIReleaseFont(MozillaHeadlineFont);
}

int main(int ArgC, char **ArgV)
{
    Rr_Config Config = {
        .WindowTitle = "UIDemo",
        .InitFunc = Init,
        .EventFunc = Event,
        .IterateFunc = Iterate,
        .CleanupFunc = Cleanup,
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
    };
    Rr_Run(&Config);

    return 0;
}
