#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <cfloat>
#include <cstdio>
#include <print>
#include <string_view>

class CUI2DemoApp
{
    bool SlowMo{};

    Rr_UIWindow *Window0{};
    Rr_UIWindow *Window1{};

public:
    CUI2DemoApp()
    {
        Window0 = Rr_UI2CreateWindow("Window0");
        Window1 = Rr_UI2CreateWindow("Window1");
    }

    void Event(Rr_Event const *Event)
    {
        if (Event->Type == RR_EVENT_TYPE_KEY_DOWN)
        {
            if (Event->Key.Scancode == RR_SCANCODE_F5)
            {
                SlowMo = !SlowMo;
                Rr_SetTargetFrameRate(SlowMo ? 2 : 180);
            }
            if (Event->Key.Scancode == RR_SCANCODE_F6)
            {
                Rr_SetPresentMode(RR_PRESENT_MODE_IMMEDIATE);
            }
        }
    }

    auto MyButton(std::string Name)
    {
        auto Item = Rr_UIButton(Name.c_str());
        Item->Padding = Rr_V2(5.0f, 3.0f);
        Item->Extents[RR_UI_AXIS_X].Rigid = 0.0f;
        Item->Extents[RR_UI_AXIS_Y].Rigid = 1.0f;

        return Item;
    }

    auto FillButton(std::string Name)
    {
        auto Item = Rr_UIButton(Name.c_str());
        Item->Padding = Rr_V2(5.0f, 3.0f);
        Item->Extents[RR_UI_AXIS_X].Rigid = 0.0f;
        Item->Extents[RR_UI_AXIS_Y].Rigid = 1.0f;
        Item->Fill = true;

        return Item;
    }

    auto MakeHori(char const *Name)
    {
        auto Hori = Rr_UIGetItem(Name);
        Hori->Axis = RR_UI_AXIS_X;
        Hori->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_SUM;
        Hori->Extents[RR_UI_AXIS_Y].Type = RR_UI_EXTENT_TYPE_SUM;
        Hori->DrawFunc = Rr_UIDrawBevel;
        Hori->DrawData = 0xAAAAAAFF;
        Hori->Padding = Rr_V2(4.0f, 4.0f);

        return Hori;
    }

    auto MakeVert(char const *Name)
    {
        auto Hori = Rr_UIGetItem(Name);
        Hori->Axis = RR_UI_AXIS_Y;
        Hori->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_SUM;
        Hori->Extents[RR_UI_AXIS_Y].Type = RR_UI_EXTENT_TYPE_SUM;
        Hori->DrawFunc = Rr_UIDrawBevel;
        Hori->DrawData = 0xAAAAAAFF;
        Hori->Padding = Rr_V2(4.0f, 4.0f);

        return Hori;
    }

    void WindowlessStuff()
    {
        auto Hori = MakeHori("Hori");
        Hori->DrawFunc = nullptr;

        Rr_UIPush(Hori);
        {
            FillButton("Total");
            FillButton("Vibecoder");
            FillButton("Death");
            FillButton("LMAO");

            auto Vert = MakeVert("Vert0");
            Vert->Padding = Rr_V2F(0.0f);

            Rr_UIPush(Vert);
            {
                FillButton("My");
                FillButton("Very");
                FillButton("Special");
                FillButton("Button");
            }
            Rr_UIPop();

            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));

            static char Buffer[128] = { "UTF8 'n shieeet\nNew line\nSOSI HUUUY" };
            auto InputField = Rr_UIInputFieldV2("MyInputField", sizeof(Buffer), Buffer);
            InputField->Padding = Rr_V2(8.0f, 8.0f);

            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));

            static char Buffer2[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed\n"
                                    "do eiusmod tempor incididunt ut labore et dolore magna aliqua.\n"
                                    "Ut enim ad minim veniam, quis nostrud exercitation ullamco\n"
                                    "laboris nisi ut aliquip ex ea commodo consequat.";
            auto InputField2 = Rr_UIInputFieldV2("MyInputField2", sizeof(Buffer2), Buffer2);
            InputField2->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PIXEL;
            InputField2->Extents[RR_UI_AXIS_X].Value = 140.0f;
            InputField2->Scrollable[RR_UI_AXIS_X] = true;
            InputField2->Padding = Rr_V2(8.0f, 8.0f);

            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));

            auto ScrollArea = Rr_UIGetItem("ScrollArea");
            ScrollArea->Axis = RR_UI_AXIS_Y;
            ScrollArea->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PIXEL;
            ScrollArea->Extents[RR_UI_AXIS_X].Value = 40.0f;
            ScrollArea->Extents[RR_UI_AXIS_Y].Type = RR_UI_EXTENT_TYPE_PIXEL;
            ScrollArea->Extents[RR_UI_AXIS_Y].Value = 40.0f;
            ScrollArea->Scrollable[RR_UI_AXIS_X] = true;
            ScrollArea->Scrollable[RR_UI_AXIS_Y] = true;
            ScrollArea->DrawFunc = Rr_UIDrawBevel;
            ScrollArea->DrawData = 0xAAAAAAFF;
            ScrollArea->Padding = Rr_V2(4.0f, 4.0f);

            Rr_UIPush(ScrollArea);
            {
                MyButton("Button###0");
                MyButton("Button###1");
                MyButton("Button###2");
                MyButton("Button###3");
            }
            Rr_UIPop();

            // if(ScrollArea->MouseWheelDelta.Y != 0.0f)
            // {
            //     std::println("sdf");
            //     ScrollArea->Scroll.Y += ScrollArea->MouseWheelDelta.Y;
            // }
            // ScrollArea->DrawText = true;
            // static char LongText[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed\n"
            //                          "do eiusmod tempor incididunt ut labore et dolore magna aliqua.\n"
            //                          "Ut enim ad minim veniam, quis nostrud exercitation ullamco\n"
            //                          "laboris nisi ut aliquip ex ea commodo consequat.";
            // ScrollArea->TextLength = sizeof(LongText);
            // ScrollArea->Text = LongText;
        }
        Rr_UIPop();

        Rr_UIInfo();
    }

    void Window0Contents()
    {
        auto Item = Rr_UIGetWindowItem(Window0);

        Rr_UIPush(Item);
        {
            auto Hori = MakeHori("HoriButs");
            Hori->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Hori->Extents[RR_UI_AXIS_X].Value = 1.0f;
            Hori->Padding = Rr_V2F(0.0f);
            Hori->DrawFunc = nullptr;
            Rr_UIPush(Hori);
            auto Button0 = MyButton("Button###0");
            Button0->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Button0->Extents[RR_UI_AXIS_X].Value = 0.3333f;
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            auto Button1 = MyButton("Button###1");
            Button1->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Button1->Extents[RR_UI_AXIS_X].Value = 0.3333f;
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            auto Button2 = MyButton("Button###2");
            Button2->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Button2->Extents[RR_UI_AXIS_X].Value = 0.3333f;
            Rr_UIPop();

            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            auto MenuButton = MyButton("Menu");
            if (MenuButton->Clicked)
            {
                Rr_UIOpenPopup(Rr_UIPopupInfo{ .Parent = MenuButton, .Anchor = RR_UI_POPUP_ANCHOR_BOTTOM });
            }
            auto MenuRoot = Rr_UIGetPopup(MenuButton);
            if (MenuRoot)
            {
                Rr_UIPush(MenuRoot);
                if (Rr_UIPushContextMenu("Menu Entry 0"))
                {
                    if (Rr_UIPushContextMenu("Menu Entry 00"))
                    {
                        Rr_UIContextMenuItem("SubMenu Entry 00");
                        Rr_UIContextMenuItem("SubMenu Entry 01");
                        Rr_UIPop();
                    }
                    Rr_UIContextMenuItem("Menu Entry 01");
                    if (Rr_UIPushContextMenu("Menu Entry Long String 02"))
                    {
                        Rr_UIContextMenuItem("SubMenu Entry 20");
                        Rr_UIContextMenuItem("SubMenu Entry 21");
                        Rr_UIPop();
                    }
                    // Rr_UISpacer(Rr_UIEm(0.1f));
                    if (Rr_UIContextMenuItem("Menu Entry 03a"))
                    {
                        std::println("sdf");
                    }
                    Rr_UIPop();
                }
                Rr_UIContextMenuItem("Menu Entry 1");
                Rr_UIPop();
            }
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            MyButton("Button###3");
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            MyButton("Button###4");
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            MyButton("Button###5");
        }
        Rr_UIPop();
    }

    void Window1Contents()
    {
        auto Item = Rr_UIGetWindowItem(Window1);

        Rr_UIPush(Item);
        {
            auto Hori = MakeHori("HoriButs");
            Hori->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Hori->Extents[RR_UI_AXIS_X].Value = 1.0f;
            Hori->Padding = Rr_V2F(0.0f);
            Hori->DrawFunc = nullptr;
            Rr_UIPush(Hori);
            auto Button0 = MyButton("Button###0");
            Button0->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Button0->Extents[RR_UI_AXIS_X].Value = 0.3333f;
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            auto Button1 = MyButton("Button###1");
            Button1->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Button1->Extents[RR_UI_AXIS_X].Value = 0.3333f;
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            auto Button2 = MyButton("Button###2");
            Button2->Extents[RR_UI_AXIS_X].Type = RR_UI_EXTENT_TYPE_PERCENT;
            Button2->Extents[RR_UI_AXIS_X].Value = 0.3333f;
            Rr_UIPop();

            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));

            MyButton("Button###3");
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            MyButton("Button###4");
            Rr_UISpacer(Rr_UIEm(0.1f, 1.0f));
            MyButton("Button###5");
        }
        Rr_UIPop();
    }

    void Iterate()
    {
        auto Graph = Rr_GetGraph();

        auto SwapchainImage = Rr_GetSwapchainImage();

        auto BackgroundColor = Rr_V4(0.33f, 0.33f, 0.46f, 1);
        Rr_ClearColorImage2D(Graph, Rr_ColorClear{ BackgroundColor }, SwapchainImage);

        WindowlessStuff();
        Window0Contents();
        Window1Contents();
    }

    ~CUI2DemoApp()
    {
    }
};

int main()
{
    static CUI2DemoApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "UI2Demo",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .SwapchainFormat = RR_IMAGE_FORMAT_B8G8R8A8_UNORM,
        .InitFunc = []() { App = new CUI2DemoApp(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
