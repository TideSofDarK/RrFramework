#include <Rr/Rr.h>

#include <array>
#include <iostream>

static bool FixedSizeWindowOpen = false;
static bool StyleEditorWindowOpen = false;

static void Init(void *UserData)
{
}

static void FixedSizeWindow()
{
    Rr_UISetNextWindowSize({ 400, 400 });
    if (Rr_UIBeginWindow(
            "Fixed Size Window",
            &FixedSizeWindowOpen,
            RR_UI_WINDOW_FLAGS_CLOSE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
                RR_UI_WINDOW_FLAGS_NO_BORDER_BIT))
    {
        Rr_UILabel("Border is disabled for this window.");
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
    Rr_Renderer *Renderer = Rr_GetRenderer();

    Rr_ColorClear ColorClear = {};
    Rr_AddClearColorImageNode(
        Renderer,
        "clear",
        &ColorClear,
        Rr_GetSwapchainImage(Renderer));

    Rr_UIDebugOverlay();

    static bool CloseButton = false;
    static bool NoResize = false;
    static bool NoScrollbar = false;
    static bool NoTitle = false;

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

    FixedSizeWindow();
    StyleEditorWindow();

    if (Rr_UIBeginWindow("Rr_UI.h", nullptr, Flags))
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
            Rr_UIBeginHorizontal();
            Rr_UICheckbox("Close Button", &CloseButton);
            Rr_UICheckbox("No Resize", &NoResize);
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
        }
        if (Rr_UIFold("Color Picker"))
        {
            static Rr_Vec4 ColorA = { 0.2f, 0.3f, 0.4f, 1.0f };
            Rr_UIColorPicker("Color A", &ColorA);
            static Rr_Vec4 ColorB = { 0.9f, 0.1f, 0.2f, 1.0f };
            Rr_UIColorPicker("Color B", &ColorB);
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
