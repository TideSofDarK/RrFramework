#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <stdio.h>
#include <stdlib.h>

static bool FixedSizeWindowOpen = false;
static bool ThemeEditorWindowOpen = false;
static bool TextInputWindowOpen = false;

static Rr_UIFont *StoneTombFont = NULL;
static Rr_UIFont *MozillaHeadlineFont = NULL;

static void TextInputWindow()
{
    if (Rr_UIBeginWindow(
            "Text Input Window",
            &TextInputWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT))
    {
        static char StringBuffer[2048] = "";
        if (Rr_UIInputField(
                "String (2048 bytes)",
                sizeof(StringBuffer),
                StringBuffer,
                "Type here...",
                NULL,
                RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT))
        {
            fprintf(stderr, "%s\n", StringBuffer);
        }
        Rr_UIEndWindow();
    }
}

static void FixedSizeWindow()
{
    const Rr_Vec2 WINDOW_SIZE = Rr_V2(450.0f, 200.0f);
    Rr_UISetNextWindowExtent(WINDOW_SIZE);
    if (Rr_UIBeginWindow(
            "Fixed Size Window",
            &FixedSizeWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT))
    {
        Rr_UIText("Resizing is disabled for this window.");
        Rr_UITextF("Window Size: %.0fx%.0f", WINDOW_SIZE.X, WINDOW_SIZE.Y);
        Rr_UIEndWindow();
    }
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
    fprintf(
        stdout,
        "Colors->%s = Rr_V4(%ff,%ff,%ff,%ff);\n",
        Name,
        Color->R,
        Color->G,
        Color->B,
        Color->A);
}

static void PrintTheme(void)
{
    Rr_UIColors *Colors = Rr_UIGetColors();
    Rr_UIStyle *Style = Rr_UIGetStyle();

    fprintf(stdout, "\n/* RR UI THEME EXPORT BEGIN */\n");

    PrintStyleVec2("TitlePadding", &Style->TitlePadding);
    PrintStyleVec2("WindowPadding", &Style->WindowPadding);
    PrintStyleVec2("ContentsMargin", &Style->ContentsMargin);
    PrintStyle("ComponentMargin", Style->ComponentMargin);
    PrintStyle("ScrollbarAreaWidth", Style->ScrollbarAreaWidth);
    PrintStyle("BevelIntensityLight", Style->BevelIntensityLight);
    PrintStyle("BevelIntensityDark", Style->BevelIntensityDark);
    PrintStyleVec2("ButtonPadding", &Style->ButtonPadding);
    PrintStyleVec2("InputFieldPadding", &Style->InputFieldPadding);
    PrintStyleVec2("CheckmarkRatios", &Style->CheckmarkRatios);
    PrintStyle("CheckmarkSize", Style->CheckmarkSize);

    PrintColor("Foreground", &Colors->Foreground);
    PrintColor("ForegroundDimmed", &Colors->ForegroundDimmed);
    PrintColor("Background", &Colors->Background);
    PrintColor("ChildBackground", &Colors->ChildBackground);
    PrintColor("Outline", &Colors->Outline);
    PrintColor("SelectedOutline", &Colors->SelectedOutline);

    PrintColor("TitleBackground", &Colors->TitleBackground);
    PrintColor(
        "TitleCloseButtonBackground",
        &Colors->TitleCloseButtonBackground);
    PrintColor(
        "TitleCollapseButtonBackground",
        &Colors->TitleCollapseButtonBackground);

    PrintColor("ScrollbarBackground", &Colors->ScrollbarBackground);
    PrintColor("ScrollbarNormal", &Colors->ScrollbarNormal);
    PrintColor("ScrollbarHovered", &Colors->ScrollbarHovered);
    PrintColor("ScrollbarHeld", &Colors->ScrollbarHeld);

    PrintColor("ButtonNormal", &Colors->ButtonNormal);
    PrintColor("ButtonHovered", &Colors->ButtonHovered);
    PrintColor("ButtonHeld", &Colors->ButtonHeld);
    PrintColor("ButtonDisabled", &Colors->ButtonDisabled);

    PrintColor("InputFieldNormal", &Colors->InputFieldNormal);
    PrintColor("InputFieldActive", &Colors->InputFieldActive);
    PrintColor("SelectedTextBackground", &Colors->SelectedTextBackground);
    PrintColor("SelectedTextForeground", &Colors->SelectedTextForeground);

    fprintf(stdout, "/* RR UI THEME EXPORT END */\n\n");
}

static void ThemeEditorWindow()
{
    if (Rr_UIBeginWindow(
            "Theme Editor",
            &ThemeEditorWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT))
    {
        Rr_UIStyle *Style = Rr_UIGetStyle();
        Rr_UIColors *Colors = Rr_UIGetColors();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2ZO("Title Padding", Style->TitlePadding.Elements);
        Rr_UIInputFloat2ZO("Window Padding", Style->WindowPadding.Elements);
        Rr_UIInputFloat2ZO("Contents Margin", Style->ContentsMargin.Elements);
        Rr_UIInputFloatZO("Component Margin", &Style->ComponentMargin);
        Rr_UIInputFloatZO("Scrollbar Area Width", &Style->ScrollbarAreaWidth);
        Rr_UIInputFloatZO("Bevel Intensity Light", &Style->BevelIntensityLight);
        Rr_UIInputFloatZO("Bevel Intensity Dark", &Style->BevelIntensityDark);
        Rr_UIInputFloat2ZO("Checkmark Ratios", Style->CheckmarkRatios.Elements);
        Rr_UIInputFloatZO("Checkmark Size", &Style->CheckmarkSize);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UISeparator();

        Rr_UIInputColor4("Foreground", Colors->Foreground.Elements);
        Rr_UIInputColor4(
            "Foreground Dimmed",
            Colors->ForegroundDimmed.Elements);
        Rr_UIInputColor4("Background", Colors->Background.Elements);
        Rr_UIInputColor4("Child Background", Colors->ChildBackground.Elements);
        Rr_UIInputColor4("Outline", Colors->Outline.Elements);
        Rr_UIInputColor4("Selected Outline", Colors->SelectedOutline.Elements);

        Rr_UISeparator();

        Rr_UIInputColor4("Title Background", Colors->TitleBackground.Elements);
        Rr_UIInputColor4(
            "Title Close Button",
            Colors->TitleCloseButtonBackground.Elements);
        Rr_UIInputColor4(
            "Title Collapse Button",
            Colors->TitleCollapseButtonBackground.Elements);

        Rr_UISeparator();

        Rr_UIInputColor4(
            "Scrollbar Background",
            Colors->ScrollbarBackground.Elements);
        Rr_UIInputColor4("Scrollbar Normal", Colors->ScrollbarNormal.Elements);
        Rr_UIInputColor4(
            "Scrollbar Hovered",
            Colors->ScrollbarHovered.Elements);
        Rr_UIInputColor4("Scrollbar Held", Colors->ScrollbarHeld.Elements);

        Rr_UISeparator();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2ZO("Button Padding", Style->ButtonPadding.Elements);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UIInputColor4("Button Normal", Colors->ButtonNormal.Elements);
        Rr_UIInputColor4("Button Hovered", Colors->ButtonHovered.Elements);
        Rr_UIInputColor4("Button Held", Colors->ButtonHeld.Elements);
        Rr_UIInputColor4("Button Disabled", Colors->ButtonDisabled.Elements);

        Rr_UISeparator();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2ZO(
            "Input Field Padding",
            Style->InputFieldPadding.Elements);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UIInputColor4(
            "Input Field Normal",
            Colors->InputFieldNormal.Elements);
        Rr_UIInputColor4(
            "Input Field Active",
            Colors->InputFieldActive.Elements);
        Rr_UIInputColor4(
            "Selected Text BG",
            Colors->SelectedTextBackground.Elements);
        Rr_UIInputColor4(
            "Selected Text FG",
            Colors->SelectedTextForeground.Elements);

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

        Rr_UIEndWindow();
    }
}

static void SetPinkTheme()
{
    Rr_UIColors *Colors = Rr_UIGetColors();
    *Colors = (Rr_UIColors){
        .Foreground = { 0.940547f, 0.899630f, 0.970019f, 1.000000f },
        .ForegroundDimmed = { 0.634900f, 0.617390f, 0.660727f, 1.000000f },
        .Background = { 0.153051f, 0.142532f, 0.186284f, 1.000000f },
        .ChildBackground = { 0.115195f, 0.100193f, 0.136111f, 1.000000f },
        .Outline = { 0.542000f, 0.495298f, 0.579593f, 1.000000f },
        .SelectedOutline = { 0.511727f, 0.396375f, 0.627315f, 1.000000f },
        .TitleBackground = { 0.472738f, 0.325686f, 0.538689f, 1.000000f },
        .TitleCloseButtonBackground = { 0.839551f,
                                        0.250613f,
                                        0.313724f,
                                        1.000000f },
        .TitleCollapseButtonBackground = { 0.470588f,
                                           0.325490f,
                                           0.537255f,
                                           1.000000f },
        .ScrollbarBackground = { 0.070520f, 0.093346f, 0.111383f, 1.000000f },
        .ScrollbarNormal = { 0.322105f, 0.261189f, 0.365981f, 1.000000f },
        .ScrollbarHovered = { 0.408665f, 0.497836f, 0.557879f, 1.000000f },
        .ScrollbarHeld = { 0.254856f, 0.342832f, 0.400486f, 1.000000f },
        .ButtonNormal = { 0.334322f, 0.277384f, 0.413703f, 1.000000f },
        .ButtonHovered = { 0.408665f, 0.497836f, 0.557879f, 1.000000f },
        .ButtonHeld = { 0.463433f, 0.343573f, 0.643222f, 1.000000f },
        .ButtonDisabled = { 0.070520f, 0.093346f, 0.111383f, 1.000000f },
        .InputFieldNormal = { 0.199295f, 0.134664f, 0.234178f, 1.000000f },
        .InputFieldActive = { 0.334506f, 0.193713f, 0.413703f, 1.000000f },
        .SelectedTextBackground = { 0.793058f,
                                    0.188728f,
                                    1.000000f,
                                    1.000000f },
        .SelectedTextForeground = { 1.000000f,
                                    1.000000f,
                                    1.000000f,
                                    1.000000f },
    };
}

static void Init(void)
{
    Rr_Asset StoneTombAsset = Rr_LoadAsset(EXAMPLE_ASSET_STONETOMB_TTF);
    StoneTombFont =
        Rr_UICreateFont(StoneTombAsset.Size, StoneTombAsset.Pointer, 18.0f);

    Rr_Asset MozillaHeadlineCleanAsset =
        Rr_LoadAsset(EXAMPLE_ASSET_MOZILLAHEADLINE_TTF);
    MozillaHeadlineFont = Rr_UICreateFont(
        MozillaHeadlineCleanAsset.Size,
        MozillaHeadlineCleanAsset.Pointer,
        14.0f);
}

static void Iterate(void)
{
    Rr_Graph *Graph = Rr_GetGraph();

    Rr_ClearColorImage2D(Graph, (Rr_ColorClear){ 0 }, Rr_GetSwapchainImage());

    Rr_UIDebugOverlay();

    static bool Open = true;
    static bool CloseButton = true;
    static bool NoResize = false;
    static bool NoScrollbar = false;
    static bool NoTitle = false;
    static bool AutoResize = true;

    Rr_UIWindowFlags Flags = 0;
    if (CloseButton)
    {
        Flags |= RR_UI_WINDOW_FLAGS_CLOSE_BIT;
    }
    if (NoResize)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT;
    }
    if (NoScrollbar)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT;
    }
    if (NoTitle)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_TITLE_BIT;
    }
    if (AutoResize)
    {
        Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;
    }

    FixedSizeWindow();
    ThemeEditorWindow();
    TextInputWindow();

    if (Rr_UIBeginWindow("Rr_UI.h - General", &Open, 0))
    {
        Rr_UISetNextWindowCreateCollapsed(false);
        if (Rr_UIBeginChild("Style and Colors"))
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

            if (Rr_UIButton("Set Pink Theme"))
            {
                SetPinkTheme();
            }

            Rr_UIEndChild();
        }

        /* Rr_UISetNextWindowCreateCollapsed(false); */
        if (Rr_UIBeginChild("Fonts"))
        {
            Rr_UIText(
                "Fonts can be dynamically pushed onto the stack. All paddings\n"
                "and margins are dependent on current fonts line height but\n"
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

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Trees"))
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

            Rr_UIEndChild();
        }

        /* if (Rr_UIBeginChild("Child Windows")) */
        /* { */
        /*     Rr_UIBeginHorizontal(); */
        /*     if (Rr_UIBeginChild("Child Window A")) */
        /*     { */
        /*         Rr_UIButton("Click Me!"); */
        /*         Rr_UIEndChild(); */
        /*     } */
        /*     if (Rr_UIBeginChild("Child Window B")) */
        /*     { */
        /*         static char Buffer[8] = { 0 }; */
        /*         Rr_UIInputField( */
        /*             "Tiny Buffer (8 bytes)", */
        /*             8, */
        /*             Buffer, */
        /*             "Type here...", */
        /*             NULL, */
        /*             0); */
        /*         Rr_UIEndChild(); */
        /*     } */
        /*     Rr_UIEndHorizontal(); */
        /*     if (Rr_UIBeginChild("Child Window C")) */
        /*     { */
        /*         Rr_UILabel("Label A"); */
        /*         Rr_UILabel("Label B"); */
        /*         Rr_UIEndChild(); */
        /*     } */
        /*     Rr_UIEndChild(); */
        /* } */

        if (Rr_UIBeginChild("Custom Draw"))
        {
            Rr_Vec2 Cursor = Rr_UIGetCursor();

            const Rr_Vec2 AREA = Rr_V2(300.0f, 200.0f);

            Rr_UIDrawCircleFilled(
                Rr_AddV2(Cursor, Rr_V2(AREA.X * 0.25f, AREA.Y * 0.5f)),
                AREA.X * 0.25f,
                &Rr_UIGetColors()->Foreground);

            Rr_UIDrawEquilateralTriangleFilled(
                Rr_AddV2(
                    Cursor,
                    Rr_V2(AREA.X * 0.75f, AREA.Y - AREA.X * 0.4f * 0.5f)),
                AREA.X * 0.4f,
                RR_ANGLE_DEG(-90.0f),
                &Rr_UIGetColors()->Foreground);

            float PhaseA = 1.0f + cosf(Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            float PhaseB =
                1.0f + cosf(RR_PI * 0.25f + Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            float PhaseC =
                1.0f + cosf(RR_PI * 0.5f + Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            float PhaseD =
                1.0f + cosf(RR_PI * 0.75f + Rr_GetTimeSeconds() * 5.0f) * 0.25f;
            Rr_UIVertex QuadVertices[4] = {
                {
                    .Position = Rr_AddV2(
                        Cursor,
                        Rr_V2(
                            AREA.X * 0.25f * PhaseA,
                            AREA.Y * 0.25f * PhaseB)),
                    .Color = Rr_V4(1.0f, 0.0f, 0.0f, 1.0f),
                },
                {
                    .Position = Rr_AddV2(
                        Cursor,
                        Rr_V2(
                            AREA.X * 0.75f * PhaseB,
                            AREA.Y * 0.25f * PhaseA)),
                    .Color = Rr_V4(0.0f, 1.0f, 0.0f, 1.0f),
                },
                {
                    .Position = Rr_AddV2(
                        Cursor,
                        Rr_V2(
                            AREA.X * 0.75f * PhaseC,
                            AREA.Y * 0.75f * PhaseD)),
                    .Color = Rr_V4(0.0f, 1.0f, 1.0f, 1.0f),
                },
                {
                    .Position = Rr_AddV2(
                        Cursor,
                        Rr_V2(
                            AREA.X * 0.25f + PhaseD,
                            AREA.Y * 0.75f * PhaseC)),
                    .Color = Rr_V4(1.0f, 0.0f, 1.0f, 1.0f),
                },
            };
            Rr_UIDrawQuadVertices(QuadVertices);

            Rr_UIAdvance(AREA);

            Rr_UIEndChild();
        }

        Rr_UIEndWindow();
    }

    if (Rr_UIBeginWindow("Rr_UI.h - Widgets", &Open, Flags))
    {
        Rr_UISetNextWindowCreateCollapsed(false);
        if (Rr_UIBeginChild("Text"))
        {
            Rr_UIText("Simple Text");
            Rr_UIText(
                " - Leading Spaces and Line Breaks 1\n - Leading Spaces and "
                "Line Breaks 2");
            Rr_UILabelText("Label", "* Line 1\n* Line 2\n* Line 3");
            Rr_UITextEx(
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. "
                "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                "laboris nisi ut aliquip ex ea commodo consequat.",
                RR_UI_TEXT_FLAGS_WRAPPED_BIT);

            Rr_UIEndChild();
        }

        Rr_UISetNextWindowCreateCollapsed(false);
        if (Rr_UIBeginChild("Buttons"))
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

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Comboboxes"))
        {
            const char *ComboboxOptions[5] = {
                "Option A", "Option B",        "Option C",
                "Option D", "Longer Option E",
            };
            static uint32_t SelectedComboboxOption = 0;
            if (Rr_UICombobox(
                    "Options",
                    RR_ARRAY_COUNT(ComboboxOptions),
                    ComboboxOptions,
                    &SelectedComboboxOption))
            {
                fprintf(
                    stderr,
                    "New option selected: %s\n",
                    ComboboxOptions[SelectedComboboxOption]);
            }
            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Checkboxes"))
        {
            Rr_UICheckbox("Close Button", &CloseButton);
            Rr_UIBeginHorizontal();
            Rr_UICheckbox("No Resize", &NoResize);
            Rr_UICheckbox("Auto Resize", &AutoResize);
            Rr_UIEndHorizontal();
            Rr_UIBeginHorizontal();
            Rr_UICheckbox("No Scrollbar", &NoScrollbar);
            Rr_UICheckbox("No Title", &NoTitle);
            Rr_UIEndHorizontal();
            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Sliders"))
        {
            static float FloatA = 0.45f;
            Rr_UISliderFloat("Float 0 to 1", &FloatA, 0.0f, 1.0f);
            static float FloatB = -16.0f;
            Rr_UISliderFloat("Float -32 to 32", &FloatB, -32.0f, 32.0f);
            static int32_t Int = 0;
            Rr_UISliderInt("Int -2 to 2", &Int, -2, 2);
            static uint32_t UnsignedInt = 0;
            Rr_UISliderUnsignedInt("Unsigned 2 to 8", &UnsignedInt, 2, 8);

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Colors"))
        {
            static Rr_Vec3 ColorRGB = { 0.2f, 0.3f, 0.4f };
            Rr_UIInputColor3("Color RGB", ColorRGB.Elements);
            static Rr_Vec4 ColorRGBA = { 0.9f, 0.2345f, 0.2f, 0.5f };
            Rr_UIInputColor4("Color RGBA", ColorRGBA.Elements);

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Input Fields"))
        {
            static char StringBuffer[16] = "Hello, World!";
            Rr_UIInputText("String (16 bytes)", 16, StringBuffer);

            static char MultilineBuffer[128] =
                "Line A\nLine B <- Delete this!\nLine C!";
            Rr_UIInputText("String (128 bytes)", 128, MultilineBuffer);

            static int32_t TestInt = 1337;
            Rr_UIInputInt("Integer Input", &TestInt);

            static int32_t TestIntRange = 5;
            Rr_UIInputIntRange("Integer Input (2-8)", &TestIntRange, 2, 8);

            static uint32_t TestUnsignedInt = 348579;
            Rr_UIInputUnsignedInt("Unsigned Input", &TestUnsignedInt);

            static uint32_t TestUnsignedIntRange = 6;
            Rr_UIInputUnsignedIntRange(
                "Unsigned Input (3-9)",
                &TestUnsignedIntRange,
                3,
                9);

            static float TestFloat = 123.456f;
            Rr_UIInputFloat("Float Input", &TestFloat);

            static float TestFloatRange = 0.25f;
            Rr_UIInputFloatRange(
                "Float Input (0-1)",
                &TestFloatRange,
                0.0f,
                1.0f);

            if (Rr_UIBeginChild("Vectors and Matrices"))
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
                Rr_UIInputFloat2x2(
                    "Float2x2 Input",
                    (float *)TestMat2.Elements);
                static Rr_Mat3 TestMat3 = {
                    1.0f,  -1.0f, 1.0f,  //
                    -1.0f, 1.0f,  -1.0f, //
                    1.0f,  -1.0f, 1.0f,  //
                };
                Rr_UIInputFloat3x3(
                    "Float3x3 Input",
                    (float *)TestMat3.Elements);
                static Rr_Mat4 TestMat4 = {
                    1.0f,  -1.0f, 1.0f,  -1.0f, //
                    -1.0f, 1.0f,  -1.0f, 1.0f,  //
                    1.0f,  -1.0f, 1.0f,  -1.0f, //
                    -1.0f, 1.0f,  -1.0f, 1.0f,  //
                };
                Rr_UIInputFloat4x4(
                    "Float4x4 Input",
                    (float *)TestMat4.Elements);

                Rr_UIEndChild();
            }

            Rr_UIEndChild();
        }

        Rr_UIEndWindow();
    }
}

static void Cleanup(void)
{
    Rr_UIReleaseFont(StoneTombFont);
    Rr_UIReleaseFont(MozillaHeadlineFont);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "UITest",
        .InitFunc = Init,
        .IterateFunc = Iterate,
        .CleanupFunc = Cleanup,
    };
    Rr_Run(&Config);

    return 0;
}
