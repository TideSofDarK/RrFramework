#include "Rr_UI.h"

#include "Rr_Memory.h"

typedef enum Rr_UIObjectType Rr_UIObjectType;
enum Rr_UIObjectType
{
    RR_UI_OBJECT_TYPE_WINDOW,
    RR_UI_OBJECT_TYPE_LABEL,
    RR_UI_OBJECT_TYPE_BUTTON,
};

typedef struct Rr_UIObject Rr_UIObject;
struct Rr_UIObject
{
    Rr_String Title;
    Rr_UIObjectType Type;
    Rr_UIObject *Next;
};

void Rr_BeginWindow(Rr_App *App, const char *Title)
{
}

void Rr_EndWindow(Rr_App *App)
{
}

void Rr_Label(Rr_App *App, const char *Text)
{
}

void Rr_Button(Rr_App *App, const char *Text)
{
}

void Rr_BeginHorizontal(Rr_App *App)
{
}

void Rr_EndHorizontal(Rr_App *App)
{
}

Rr_UI *Rr_CreateUI(Rr_App *App)
{
    Rr_Arena *Arena = Rr_CreateArenaDefault();

    Rr_UI *UI = RR_ARENA_ALLOC_ONE(Arena, sizeof(Rr_UI));

    UI->Arena = Arena;

    return UI;
}

void Rr_DestroyUI(Rr_App *App, Rr_UI *UI)
{
    Rr_DestroyArena(UI->Arena);
}

void Rr_ResetUI(Rr_App *App, Rr_UI *UI)
{
    /* @TODO: Formalize. */
    UI->Arena->Position = sizeof(Rr_UI);
}

void Rr_DrawUI(Rr_App *App, Rr_UI *UI)
{

}
