#include <Rr/Rr.h>

#include <stdio.h>

static bool FixedSizeWindowOpen = false;
static bool StyleEditorWindowOpen = false;
static bool TextInputWindowOpen = false;

static void TextInputWindow()
{
    if (Rr_UIBeginWindow(
            "Text Input Window",
            &TextInputWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT))
    {
        static char StringBuffer[2048] = "";
        if (Rr_UIInputField(
                "###LargeString",
                2048,
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
    Rr_UISetNextWindowSize(Rr_V2(400, 400));
    if (Rr_UIBeginWindow(
            "Fixed Size Window",
            &FixedSizeWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT))
    {
        Rr_UILabel("Resizing is disabled for this window.");
        Rr_UILabelF("Window Size: %dx%d", 400, 400);
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
        float FontSize = Rr_UIGetFontSize();
        bool UpdateFontSize = false;
        UpdateFontSize |= Rr_UIInputFloat("Font Size", &FontSize);
        UpdateFontSize |=
            Rr_UIInputFloat2("Title Padding", Style->TitlePadding.Elements);
        UpdateFontSize |= Rr_UIInputFloat2(
            "Contents Padding",
            Style->ContentsPadding.Elements);
        UpdateFontSize |= Rr_UIInputFloat(
            "Bevel Intensity Light",
            &Style->BevelIntensityLight);
        UpdateFontSize |=
            Rr_UIInputFloat("Bevel Intensity Dark", &Style->BevelIntensityDark);
        if (UpdateFontSize)
        {
            Rr_UISetFontSize(FontSize);
        }
        Rr_UISeparator();
        Rr_UIColorPicker("Foreground", &Style->Foreground);
        Rr_UIColorPicker("Foreground Dimmed", &Style->ForegroundDimmed);
        Rr_UIColorPicker("Background", &Style->Background);
        Rr_UIColorPicker("Outline", &Style->Outline);
        Rr_UIColorPicker("Selected Text", &Style->SelectedTextBackground);
        Rr_UISeparator();
        Rr_UIColorPicker("Title Background", &Style->TitleBackground);
        Rr_UIColorPicker(
            "Title Close Button",
            &Style->TitleCloseButtonBackground);
        Rr_UIColorPicker(
            "Title Collapse Button",
            &Style->TitleCollapseButtonBackground);
        Rr_UISeparator();
        Rr_UIColorPicker("Scrollbar Background", &Style->ScrollbarBackground);
        Rr_UIColorPicker("Scrollbar Normal", &Style->ScrollbarNormal);
        Rr_UIColorPicker("Scrollbar Hovered", &Style->ScrollbarHovered);
        Rr_UIColorPicker("Scrollbar Held", &Style->ScrollbarHeld);
        Rr_UISeparator();
        Rr_UIColorPicker("Button Normal", &Style->ButtonNormal);
        Rr_UIColorPicker("Button Hovered", &Style->ButtonHovered);
        Rr_UIColorPicker("Button Held", &Style->ButtonHeld);
        Rr_UIColorPicker("Button Disabled", &Style->ButtonDisabled);
        Rr_UIEndWindow();
    }
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
    static bool AutoResize = false;

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
        if (Rr_UIBeginChild("Child Windows"))
        {
            Rr_UIBeginHorizontal();
            if (Rr_UIBeginChild("Child Window A"))
            {
                Rr_UIButton("Click Me!");
                Rr_UIEndChild();
            }
            if (Rr_UIBeginChild("Child Window B"))
            {
                static char Buffer[8] = { 0 };
                Rr_UIInputField(
                    "Tiny Buffer (8 bytes)",
                    8,
                    Buffer,
                    "Type here...",
                    NULL,
                    0);
                Rr_UIEndChild();
            }
            Rr_UIEndHorizontal();
            if (Rr_UIBeginChild("Child Window C"))
            {
                Rr_UILabel("Label A");
                Rr_UILabel("Label B");
                Rr_UIEndChild();
            }
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
        if (Rr_UIBeginChild("Color Picker"))
        {
            static Rr_Vec4 ColorA = { 0.2f, 0.3f, 0.4f, 1.0f };
            Rr_UIColorPicker("Color A", &ColorA);
            static Rr_Vec4 ColorB = { 0.9f, 0.1f, 0.2f, 1.0f };
            Rr_UIColorPicker("Color B", &ColorB);
            Rr_UIEndChild();
        }
        if (Rr_UIBeginChild("Text Input"))
        {
            static char StringBuffer[16] = "Hello, World!";
            Rr_UIInputText("UTF-8 String (16 bytes)", 16, StringBuffer);
            static char MultilineBuffer[128] =
                "Line A\nLine B <- Delete this!\nLine C!";
            Rr_UIInputText("UTF-8 String (128 bytes)", 128, MultilineBuffer);
            static int32_t TestInt = 1337;
            Rr_UIInputInt("Integer Input", &TestInt);
            static float TestFloat = 123.456f;
            Rr_UIInputFloat("Float Input", &TestFloat);
            static Rr_Vec2 TestVec2 = { 1.0f, 0.0f };
            Rr_UIInputFloat2("2-Component Vector Input", TestVec2.Elements);
            static Rr_Vec3 TestVec3 = { 1.0f, 0.0f, 1.0f };
            Rr_UIInputFloat3("3-Component Vector Input", TestVec3.Elements);
            static Rr_Vec4 TestVec4 = { 1.0f, 0.0f, 1.0f, 1.0f };
            Rr_UIInputFloat4("4-Component Vector Input", TestVec4.Elements);
            Rr_UIEndChild();
        }
        if (Rr_UIBeginChild("Button"))
        {
            Rr_UIBeginHorizontal();
            if (Rr_UIButton("Show Style Editor"))
            {
                StyleEditorWindowOpen = true;
            }
            if (Rr_UIButton("Show Fixed Size Window"))
            {
                FixedSizeWindowOpen = true;
            }
            Rr_UIEndHorizontal();
            if (Rr_UIButton("Show Text Input Window"))
            {
                TextInputWindowOpen = true;
            }
            Rr_UIEndChild();
        }
        if (Rr_UIBeginChild("Text"))
        {
            Rr_UILabel("Text");
            Rr_UILabel("Multi\n line\n  text");
            Rr_UILabelEx(
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. "
                "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                "laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
                "irure dolor in reprehenderit in voluptate velit esse cillum "
                "dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
                "cupidatat non proident, sunt in culpa qui officia deserunt "
                "mollit anim id est laborum. ",
                RR_UI_TEXT_FLAGS_WRAPPED_BIT);
            Rr_UIEndChild();
        }
        if (Rr_UIBeginChild("Horizontal Layout"))
        {
            Rr_UILabel("Horizontal Layout");
            Rr_UIBeginHorizontal();
            static bool DoNothing;
            static bool DoNothing2;
            Rr_UICheckbox("Do Nothing", &DoNothing);
            if (Rr_UIButton("Do Something!"))
            {
            }
            Rr_UICheckbox("Do Nothing 2", &DoNothing2);
            Rr_UIEndHorizontal();
            Rr_UIEndChild();
        }
        Rr_UIEndWindow();
    }
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "UITest",
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
