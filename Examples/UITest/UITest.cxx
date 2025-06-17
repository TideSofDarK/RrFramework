#include <Rr/Rr.h>

#include <array>
#include <iostream>
#include <print>

static bool FixedSizeWindowOpen = false;
static bool StyleEditorWindowOpen = false;
static bool TextInputWindowOpen = false;

static void Init(void *UserData)
{
}

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
            std::println("{}", StringBuffer);
        }
        Rr_UIEndWindow();
    }
}

static void FixedSizeWindow()
{
    Rr_UISetNextWindowSize({ 400, 400 });
    if (Rr_UIBeginWindow(
            "Fixed Size Window",
            &FixedSizeWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT))
    {
        Rr_UILabel("Resize is disabled for this window.");
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
        Rr_UILabel("TEMPORARY LONG STRING -- WILL REMOVE");
        Rr_UISeparator();
        Rr_UIColorPicker("Foreground", &Style->Foreground);
        Rr_UIColorPicker("Background", &Style->Background);
        Rr_UIColorPicker("Title Background", &Style->TitleBackground);
        Rr_UIColorPicker("Outline", &Style->Outline);
        Rr_UISeparator();
        Rr_UIColorPicker("Scrollbar Background", &Style->Outline);
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

static void Iterate(void *UserData)
{
    Rr_Graph *Graph = Rr_GetGraph();

    Rr_ColorClear ColorClear = {};
    Rr_AddClearColorImageNode(
        Graph,
        "clear",
        &ColorClear,
        Rr_GetSwapchainImage());

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
        Flags |= RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT;
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
        if (Rr_UIFold("Combobox"))
        {
            std::array ComboboxOptions = {
                "Option A", "Option B",        "Option C",
                "Option D", "Longer Option E",
            };
            static uint32_t SelectedComboboxOption = 0;
            if (Rr_UICombobox(
                    "Options",
                    ComboboxOptions.size(),
                    ComboboxOptions.data(),
                    &SelectedComboboxOption))
            {
                std::cout << "New option selected: "
                          << ComboboxOptions[SelectedComboboxOption] << '\n';
            }
        }
        if (Rr_UIFold("Checkbox"))
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
        }
        if (Rr_UIFold("Slider"))
        {
            static float Float01 = 0.5f;
            Rr_UISliderFloat("Float 0 to 1", &Float01, 0.0f, 1.0f);
            static float Float22 = -0.5f;
            Rr_UISliderFloat("Float -2 to 2", &Float22, -2.0f, 2.0f);
            static std::int32_t Int18 = 0;
            Rr_UISliderInt("Int -1 to 8", &Int18, -1, 8);
        }
        if (Rr_UIFold("Color Picker"))
        {
            static Rr_Vec4 ColorA = { 0.2f, 0.3f, 0.4f, 1.0f };
            Rr_UIColorPicker("Color A", &ColorA);
            static Rr_Vec4 ColorB = { 0.9f, 0.1f, 0.2f, 1.0f };
            Rr_UIColorPicker("Color B", &ColorB);
        }
        if (Rr_UIFold("Text Input"))
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
            Rr_UIInputFloat2("Vec2 Input", TestVec2.Elements);
            static Rr_Vec3 TestVec3 = { 1.0f, 0.0f, 1.0f };
            Rr_UIInputFloat3("Vec3 Input", TestVec3.Elements);
            static Rr_Vec4 TestVec4 = { 1.0f, 0.0f, 1.0f, 1.0f };
            Rr_UIInputFloat4("Vec4 Input", TestVec4.Elements);
        }
        Rr_UILabel("Button");
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
        Rr_UISeparator();
        Rr_UILabel("Text");
        Rr_UILabel("Multi\n line\n  text");
        Rr_UILabelEx(
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
            "do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
            "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
            "laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
            "irure dolor in reprehenderit in voluptate velit esse cillum "
            "dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
            "cupidatat non proident, sunt in culpa qui officia deserunt "
            "mollit anim id est laborum. ",
            RR_UI_TEXT_FLAGS_WRAPPED_BIT);
        Rr_UISeparator();
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
        Rr_UIEndWindow();
    }
}

static void Cleanup(void *UserData)
{
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {};
    Config.Title = "UITest";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.uitest";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);

    return 0;
}
