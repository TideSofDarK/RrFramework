#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <stdio.h>

static bool FixedSizeWindowOpen = false;
static bool StyleEditorWindowOpen = false;
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

static void StyleEditorWindow()
{
    if (Rr_UIBeginWindow(
            "Style Editor",
            &StyleEditorWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT | RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT))
    {
        Rr_UIStyle *Style = Rr_UIGetStyle();
        Rr_UIColors *Colors = Rr_UIGetColors();

        /* Rr_UIInputFloat("Font Size", &FontSize); */

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2("Title Padding", Style->TitlePadding.Elements);
        Rr_UIInputFloat2("Window Padding", Style->WindowPadding.Elements);
        Rr_UIInputFloat2("Contents Margin", Style->ContentsMargin.Elements);
        Rr_UIInputFloat("Component Margin", &Style->ComponentMargin);
        Rr_UIInputFloat("Scrollbar Area Width", &Style->ScrollbarAreaWidth);
        Rr_UIInputFloat("Bevel Intensity Light", &Style->BevelIntensityLight);
        Rr_UIInputFloat("Bevel Intensity Dark", &Style->BevelIntensityDark);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UISeparator();

        Rr_UIInputColor4("Foreground", Colors->Foreground.Elements);
        Rr_UIInputColor4(
            "Foreground Dimmed",
            Colors->ForegroundDimmed.Elements);
        Rr_UIInputColor4("Background", Colors->Background.Elements);
        Rr_UIInputColor4("Child Background", Colors->ChildBackground.Elements);
        Rr_UIInputColor4("Outline", Colors->Outline.Elements);

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
        Rr_UIInputFloat2("Button Padding", Style->ButtonPadding.Elements);
        Rr_UIPopFormatFloatDecimalPlaces();

        Rr_UIInputColor4("Button Normal", Colors->ButtonNormal.Elements);
        Rr_UIInputColor4("Button Hovered", Colors->ButtonHovered.Elements);
        Rr_UIInputColor4("Button Held", Colors->ButtonHeld.Elements);
        Rr_UIInputColor4("Button Disabled", Colors->ButtonDisabled.Elements);

        Rr_UISeparator();

        Rr_UIPushFormatFloatDecimalPlaces(4);
        Rr_UIInputFloat2(
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
        if (Rr_UIButton("Print Colors"))
        {
            Rr_UIPrintColors();
        }
        if (Rr_UIButton("Randomize Colors"))
        {
            Rr_UIRandomizeColors();
        }
        Rr_UIEndHorizontal();

        Rr_UIEndWindow();
    }
}

static void Init(void)
{
    Rr_Asset StoneTombAsset = Rr_LoadAsset(EXAMPLE_ASSET_STONETOMB_TTF);
    StoneTombFont =
        Rr_UICreateFont(StoneTombAsset.Size, StoneTombAsset.Pointer, 18.0f);

    Rr_Asset MozillaHeadlineCleanAsset = Rr_LoadAsset(EXAMPLE_ASSET_MOZILLAHEADLINE_TTF);
    MozillaHeadlineFont =
        Rr_UICreateFont(MozillaHeadlineCleanAsset.Size, MozillaHeadlineCleanAsset.Pointer, 14.0f);
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
    StyleEditorWindow();
    TextInputWindow();

    if (Rr_UIBeginWindow("Rr_UI.h", &Open, Flags))
    {
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

        if (Rr_UIBeginChild("Tree"))
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
                Rr_Vec2 Vec2 = { -75.0f, 125.0f };
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

            Rr_UIPushFont(StoneTombFont);
            Rr_UIText("Different font, boo!");
            Rr_UIPopFont();

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Button"))
        {
            Rr_UIPushFont(StoneTombFont);
            if (Rr_UIButton("Show Style Editor"))
            {
                StyleEditorWindowOpen = true;
            }
            Rr_UIPopFont();

            Rr_UIPushFont(MozillaHeadlineFont);
            if (Rr_UIButton("Show Fixed Size Window"))
            {
                FixedSizeWindowOpen = true;
            }
            Rr_UIPopFont();

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

        if (Rr_UIBeginChild("Combobox"))
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

        if (Rr_UIBeginChild("Checkbox"))
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

        if (Rr_UIBeginChild("Slider"))
        {
            static float Float01 = 0.5f;
            Rr_UISliderFloat("Float 0 to 1", &Float01, 0.0f, 1.0f);
            static float Float22 = -0.5f;
            Rr_UISliderFloat("Float -2 to 2", &Float22, -2.0f, 2.0f);
            static int32_t Int18 = 0;
            Rr_UISliderInt("Int -1 to 8", &Int18, -1, 8);

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Color Input"))
        {
            static Rr_Vec3 ColorRGB = { 0.2f, 0.3f, 0.4f };
            Rr_UIInputColor3("Color RBA", ColorRGB.Elements);
            static Rr_Vec4 ColorRGBA = { 0.9f, 0.2345f, 0.2f, 0.5f };
            Rr_UIInputColor4("Color RGBA", ColorRGBA.Elements);

            Rr_UIEndChild();
        }

        if (Rr_UIBeginChild("Input Fields"))
        {
            Rr_UIPushFont(MozillaHeadlineFont);
            static char StringBuffer[16] = "Hello, World!";
            Rr_UIInputText("String (16 bytes)", 16, StringBuffer);
            static char MultilineBuffer[128] =
                "Line A\nLine B <- Delete this!\nLine C!";
            Rr_UIInputText("String (128 bytes)", 128, MultilineBuffer);
            static int32_t TestInt = 1337;
            Rr_UIInputInt("Integer Input", &TestInt);
            static uint32_t TestUnsignedInt = 348579;
            Rr_UIInputUnsignedInt("Unsigned Input", &TestUnsignedInt);
            static float TestFloat = 123.456f;
            Rr_UIInputFloat("Float Input", &TestFloat);
            Rr_UIPopFont();

            Rr_UISetNextWindowCreateCollapsed(false);
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
