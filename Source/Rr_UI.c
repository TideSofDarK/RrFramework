#include "Rr_BuiltinAssets.inc"

#include "Rr_UI.h"

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

struct Rr_UIContext
{
    Rr_UIStyle Style;
    Rr_Map *WindowMap;
    Rr_UIWindow *Window;
    Rr_Vec2 Cursor;
    Rr_Vec2 ScreenSize;
    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Buffer *Buffer;
    Rr_Font *Font;
    float FontSize;
    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *Global;

static float Rr_GetWindowTitleHeight(void)
{
    return Global->Style.TitlePadding * Global->FontSize +
           Global->Font->LineHeight * Global->FontSize;
}

void Rr_BeginWindow(const char *Title)
{
    XXH64_hash_t Hash = XXH3_64bits(Title, strlen(Title));

    Rr_UIWindow **Window = RR_UPSERT(&Global->WindowMap, Hash, Global->Arena);

    if(*Window == NULL)
    {
        *Window = RR_ALLOC(Global->Arena, sizeof(Rr_UIWindow));
        Global->Window = *Window;
        Global->Window->Position = (Rr_Vec2){
            .X = Global->ScreenSize.Width / 2.0f,
            .Y = Global->ScreenSize.Height / 2.0f,
        };
    }
    else
    {
        Global->Window = *Window;
    }

    RR_ZERO(Global->Window->Size);

    Global->Cursor.X =
        Global->Window->Position.X + Global->Style.ContentsPadding * Global->FontSize;
    Global->Cursor.Y = Global->Window->Position.Y +
                    Global->Style.ContentsPadding * Global->FontSize +
                    Rr_GetWindowTitleHeight();
}

void Rr_EndWindow(void)
{
    Global->Window = NULL;
}

static Rr_UIWidget *Rr_PushWidget(Rr_UIWindow *Window, Rr_UIWidgetType Type)
{
    if(Window->CurrentWidget == NULL)
    {
        Window->FirstWidget = RR_ALLOC(Global->FrameArena, sizeof(Rr_UIWidget));
        Window->CurrentWidget = Window->FirstWidget;
    }
    else
    {
        Window->CurrentWidget->Next =
            RR_ALLOC(Global->FrameArena, sizeof(Rr_UIWidget));
        Window->CurrentWidget = Window->CurrentWidget->Next;
    }

    Window->CurrentWidget->Type = Type;

    return Window->CurrentWidget;
}

void Rr_Label(const char *Text)
{
    if(Global->Window == NULL)
    {
        return;
    }

    Rr_UIWidget *Widget = Rr_PushWidget(Global->Window, RR_UI_WIDGET_TYPE_LABEL);
    Rr_UILabel *Label = (Rr_UILabel *)&Widget->Union.Label;
    Label->Text = Rr_CreateString(Text, 0, Global->FrameArena);
    Rr_Vec2 Size =
        Rr_CalculateTextSize(Global->Font, Global->Font->DefaultSize, &Label->Text);

    Widget->Position = Global->Cursor;
    Global->Cursor = Rr_AddV2(Global->Cursor, Size);

    Global->Window->Size.Width = RR_MAX(Global->Window->Size.Width, Size.Width);
    Global->Window->Size.Height += Size.Height;
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

Rr_UIContext *Rr_CreateUIContext(Rr_App *App)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_UIContext *Context = RR_ALLOC(Arena, sizeof(Rr_UIContext));
    Context->Arena = Arena;

    Context->Style = (Rr_UIStyle){
        .TitlePadding = 0.1f,
        .ContentsPadding = 0.1f,
        .OutlineColor = (Rr_Vec4){ 0.2f, 0.67f, 0.111f, 1.0f },
        .TitleBGColor = (Rr_Vec4){ 0.397f, 0.37f, 0.711f, 1.0f },
        .ContentsBGColor = (Rr_Vec4){ 0.1f, 0.12f, 0.114f, 1.0f },
    };

    Rr_PipelineBinding Bindings[] = {
        { 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        { 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
    };
    Rr_PipelineBindingSet BindingSets[] = {
        {
            RR_ARRAY_COUNT(Bindings),
            Bindings,
            RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    Context->PipelineLayout = Rr_CreatePipelineLayout(
        Renderer,
        RR_ARRAY_COUNT(BindingSets),
        BindingSets);

    Rr_ColorTargetInfo ColorTargets[] = {
        {
            .Format = RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            .Blend.BlendEnable = true,
            .Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA,
            .Blend.DstColorBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .Blend.ColorBlendOp = RR_BLEND_OP_ADD,
            .Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA,
            .Blend.DstAlphaBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .Blend.AlphaBlendOp = RR_BLEND_OP_ADD,
        },
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .Layout = Context->PipelineLayout,
        .VertexShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_VERT_SPV),
        .FragmentShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_FRAG_SPV),
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
    };

    Context->GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    Context->Buffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(16),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT |
            RR_BUFFER_FLAGS_STAGING_BIT);

    return Context;
}

void Rr_DestroyUIContext(Rr_App *App, Rr_UIContext *Context)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    Rr_DestroyBuffer(Renderer, Context->Buffer);
    Rr_DestroyPipelineLayout(Renderer, Context->PipelineLayout);
    Rr_DestroyGraphicsPipeline(Renderer, Context->GraphicsPipeline);
    Rr_DestroyArena(Context->Arena);
}

void Rr_BeginUI(Rr_App *App, Rr_UIContext *Context)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    // UI->Arena->Position = sizeof(Rr_UI);
    Global = Context;
    Global->Window = NULL;
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    Global->ScreenSize.Width = (float)SwapchainSize.Width;
    Global->ScreenSize.Height = (float)SwapchainSize.Height;
    Global->FrameArena = Rr_GetCurrentFrame(Renderer)->Arena;
    Global->Font = Renderer->BuiltinFont;
}

void Rr_EndUI(Rr_App *App, Rr_UIContext *Context)
{
    // Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    // Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    // Rr_GraphNode *Node = Rr_AddGraphicsNode(Renderer);
}
