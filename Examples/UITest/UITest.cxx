#include <Rr/Rr.h>

#include <array>
#include <iostream>

static void Init(void *UserData)
{
}

static void FixedSizeWindow()
{
    Rr_UISetNextWindowSize({ 400, 400 });
    Rr_UIBeginWindow(
        "Fixed Size Window",
        RR_UI_WINDOW_FLAGS_CLOSE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
            RR_UI_WINDOW_FLAGS_CREATE_CLOSED_BIT |
            RR_UI_WINDOW_FLAGS_NO_BORDER_BIT);
    Rr_UILabel("Border is disabled for this window.");
    Rr_UILabelF("Window Size: %dx%d", 400, 400);
    Rr_UIEndWindow();
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
    if(CloseButton)
    {
        Flags |= RR_UI_WINDOW_FLAGS_CLOSE_BIT;
    }
    if(NoResize)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT;
    }
    if(NoScrollbar)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT;
    }
    if(NoTitle)
    {
        Flags |= RR_UI_WINDOW_FLAGS_NO_TITLE_BIT;
    }

    FixedSizeWindow();

    Rr_UIBeginWindow("Rr_UI.h", Flags);
    if(Rr_UIFold("Combobox"))
    {
        std::array ComboboxOptions = {
            "Option A", "Option B", "Option C", "Option D", "Longer Option E",
        };
        static uint32_t SelectedComboboxOption = 0;
        if(Rr_UICombobox(
               "Options",
               ComboboxOptions.size(),
               ComboboxOptions.data(),
               &SelectedComboboxOption))
        {
            std::cout << "New option selected: "
                      << ComboboxOptions[SelectedComboboxOption] << '\n';
        }
    }
    if(Rr_UIFold("Checkbox"))
    {
        Rr_UICheckbox("Close Button", &CloseButton);
        Rr_UICheckbox("No Resize", &NoResize);
        Rr_UICheckbox("No Scrollbar", &NoScrollbar);
        Rr_UICheckbox("No Title", &NoTitle);
    }
    if(Rr_UIFold("Color Picker"))
    {
        static Rr_Vec4 ColorA = { 0.2f, 0.3f, 0.4f, 1.0f };
        Rr_UIColorPicker("Color A", &ColorA);
        static Rr_Vec4 ColorB = { 0.9f, 0.1f, 0.2f, 1.0f };
        Rr_UIColorPicker("Color B", &ColorB);
    }
    Rr_UILabel("Button");
    Rr_UIBeginHorizontal();
    if(Rr_UIButton("Show Style Editor"))
    {
    }
    if(Rr_UIButton("Show Fixed Size Window"))
    {
        Rr_UISetWindowClosed("Fixed Size Window", false);
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
    if(Rr_UIButton("Do Something!"))
    {
    }
    Rr_UICheckbox("Do Nothing 2", &DoNothing2);
    Rr_UIEndHorizontal();
    Rr_UIEndWindow();
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
