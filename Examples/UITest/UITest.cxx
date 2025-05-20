#include <Rr/Rr.h>

static void Init(void *UserData)
{
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

    Rr_DebugOverlay();

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

    Rr_BeginWindow("Rr_UI.h", Flags);
    Rr_Label("Combobox");
    static const char *ComboboxOptions[] = {
        "Option A",
        "Option B",
        "Option C",
        "Option D",
    };
    static uint32_t SelectedComboboxOption = 0;
    if(Rr_Combobox(
           "Title",
           RR_ARRAY_COUNT(ComboboxOptions),
           ComboboxOptions,
           &SelectedComboboxOption))
    {
    }
    Rr_Separator();
    Rr_Label("Checkbox");
    Rr_Checkbox("Close Button", &CloseButton);
    Rr_Checkbox("No Resize", &NoResize);
    Rr_Checkbox("No Scrollbar", &NoScrollbar);
    Rr_Checkbox("No Title", &NoTitle);
    Rr_Separator();
    Rr_Label("Button");
    Rr_BeginHorizontal();
    if(Rr_Button("Show Style Editor"))
    {
    }
    if(Rr_Button("Show Another Window"))
    {
    }
    Rr_EndHorizontal();
    Rr_Separator();
    Rr_Label("Text");
    Rr_Label("Multi\n line\n  text");
    Rr_LabelEx(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
        "do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco "
        "laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
        "irure dolor in reprehenderit in voluptate velit esse cillum "
        "dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
        "cupidatat non proident, sunt in culpa qui officia deserunt "
        "mollit anim id est laborum. ",
        RR_UI_TEXT_FLAGS_WRAPPED_BIT);
    Rr_Separator();
    Rr_Label("Horizontal Layout");
    Rr_BeginHorizontal();
    static bool DoNothing;
    static bool DoNothing2;
    Rr_Checkbox("Do Nothing", &DoNothing);
    if(Rr_Button("Do Something!"))
    {
    }
    Rr_Checkbox("Do Nothing 2", &DoNothing2);
    Rr_EndHorizontal();
    Rr_EndWindow();
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
