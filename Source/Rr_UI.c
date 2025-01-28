#include "Rr_UI.h"

#include "Rr/Rr_App.h"
#include "Rr/Rr_Text.h"
#include "Rr_App.h"
#include "Rr_Memory.h"
#include "Rr_Renderer.h"

#include <xxHash/xxhash.h>

typedef enum
{
    RR_UI_WIDGET_TYPE_LABEL,
    RR_UI_WIDGET_TYPE_BUTTON,
} Rr_UIWidgetType;

typedef struct Rr_UILabel Rr_UILabel;
struct Rr_UILabel
{
    Rr_String Text;
};

typedef struct Rr_UIButton Rr_UIButton;
struct Rr_UIButton
{
    Rr_String Text;
};

typedef struct Rr_UIWidget Rr_UIWidget;
struct Rr_UIWidget
{
    union
    {
        Rr_UILabel Label;
        Rr_UIButton Button;
    } Union;
    Rr_Vec2 Position;
    Rr_UIWidgetType Type;
    Rr_UIWidget *Next;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    Rr_String Text;
    Rr_Vec2 Position;
    Rr_Vec2 Size;
    bool Minimized;
    Rr_UIWidget *FirstWidget;
    Rr_UIWidget *CurrentWidget;
};

struct Rr_UI
{
    Rr_UIStyle Style;
    Rr_Map *WindowMap;
    Rr_UIWindow *Window;
    Rr_Vec2 Cursor;
    Rr_Vec2 ScreenSize;
    Rr_Font *Font;
    float FontSize;
    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UI *GUI;

static float Rr_GetWindowTitleHeight(void)
{
    return GUI->Style.TitlePadding * GUI->FontSize + GUI->Font->LineHeight * GUI->FontSize;
}

void Rr_BeginWindow(const char *Title)
{
    XXH64_hash_t Hash = XXH3_64bits(Title, strlen(Title));

    Rr_UIWindow **Window = RR_UPSERT(&GUI->WindowMap, Hash, GUI->Arena);

    if(*Window == NULL)
    {
        *Window = RR_ALLOC(GUI->Arena, sizeof(Rr_UIWindow));
        GUI->Window = *Window;
        GUI->Window->Position = (Rr_Vec2){
            .X = GUI->ScreenSize.Width / 2.0f,
            .Y = GUI->ScreenSize.Height / 2.0f,
        };
    }
    else
    {
        GUI->Window = *Window;
    }

    RR_ZERO(GUI->Window->Size);

    GUI->Cursor.X = GUI->Window->Position.X + GUI->Style.ContentsPadding * GUI->FontSize;
    GUI->Cursor.Y = GUI->Window->Position.Y + GUI->Style.ContentsPadding * GUI->FontSize + Rr_GetWindowTitleHeight();
}

void Rr_EndWindow(void)
{
    GUI->Window = NULL;
}

static Rr_UIWidget *Rr_PushWidget(Rr_UIWindow *Window, Rr_UIWidgetType Type)
{
    if(Window->CurrentWidget == NULL)
    {
        Window->FirstWidget = RR_ALLOC(GUI->FrameArena, sizeof(Rr_UIWidget));
        Window->CurrentWidget = Window->FirstWidget;
    }
    else
    {
        Window->CurrentWidget->Next = RR_ALLOC(GUI->FrameArena, sizeof(Rr_UIWidget));
        Window->CurrentWidget = Window->CurrentWidget->Next;
    }

    Window->CurrentWidget->Type = Type;

    return Window->CurrentWidget;
}

void Rr_Label(const char *Text)
{
    if(GUI->Window == NULL)
    {
        return;
    }

    Rr_UIWidget *Widget = Rr_PushWidget(GUI->Window, RR_UI_WIDGET_TYPE_LABEL);
    Rr_UILabel *Label = (Rr_UILabel *)&Widget->Union.Label;
    Label->Text = Rr_CreateString(Text, 0, GUI->FrameArena);
    Rr_Vec2 Size = Rr_CalculateTextSize(GUI->Font, GUI->Font->DefaultSize, &Label->Text);

    Widget->Position = GUI->Cursor;
    GUI->Cursor = Rr_AddV2(GUI->Cursor, Size);

    GUI->Window->Size.Width = RR_MAX(GUI->Window->Size.Width, Size.Width);
    GUI->Window->Size.Height += Size.Height;
}

void Rr_Button(const char *Text)
{
}

void Rr_BeginHorizontal(void)
{
}

void Rr_EndHorizontal(void)
{
}

Rr_UI *Rr_CreateUI(Rr_App *App)
{
    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_UI *UI = RR_ALLOC(Arena, sizeof(Rr_UI));

    UI->Arena = Arena;

    UI->Style = (Rr_UIStyle){
        .TitlePadding = 0.1f,
        .ContentsPadding = 0.1f,
        .OutlineColor = (Rr_Vec4){ 0.2f, 0.67f, 0.111f, 1.0f },
        .TitleBGColor = (Rr_Vec4){ 0.397f, 0.37f, 0.711f, 1.0f },
        .ContentsBGColor = (Rr_Vec4){ 0.1f, 0.12f, 0.114f, 1.0f },
    };

    return UI;
}

void Rr_DestroyUI(Rr_App *App, Rr_UI *UI)
{
    Rr_DestroyArena(UI->Arena);
}

void Rr_BeginUI(Rr_App *App, Rr_UI *UI)
{
    // UI->Arena->Position = sizeof(Rr_UI);
    GUI = UI;
    GUI->Window = NULL;
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(App);
    GUI->ScreenSize.Width = SwapchainSize.Width;
    GUI->ScreenSize.Height = SwapchainSize.Height;
    GUI->FrameArena = Rr_GetCurrentFrame(&App->Renderer)->Arena;
    GUI->Font = App->Renderer.BuiltinFont;
}

void Rr_DrawUI(Rr_App *App, Rr_UI *UI)
{
}
