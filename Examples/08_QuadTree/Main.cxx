#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <vector>

struct SPoint
{
    float X, Y;

    bool operator==(const SPoint &Rhs) const
    {
        return X == Rhs.X && Y == Rhs.Y;
    }

    friend std::ostream &operator<<(std::ostream &Stream, const SPoint &Point)
    {
        Stream << "X: " << Point.X << " Y: " << Point.Y;
        return Stream;
    }
};

struct SRect
{
    float Left, Top, Right, Bottom;

    constexpr bool Contains(const SPoint &Point) const
    {
        return Point.X >= Left && Point.X <= Right && Point.Y >= Top &&
               Point.Y <= Bottom;
    }

    constexpr bool Contains(const SRect &Inner) const
    {
        return Inner.Left >= Left && Inner.Right <= Right && Inner.Top >= Top &&
               Inner.Bottom <= Bottom;
    }

    constexpr bool Intersects(const SRect &Another) const
    {
        return ((Another.Left <= Right) && (Another.Right >= Left)) &&
               ((Another.Top <= Bottom) && (Another.Bottom >= Top));
    }

    constexpr SPoint Center() const
    {
        return { (Right + Left) / 2, (Top + Bottom) / 2 };
    }

    constexpr SPoint Extent() const
    {
        return { std::abs(Left - Right) / 2, std::abs(Bottom - Top) / 2 };
    }

    SRect &operator|(const SPoint &Rhs)
    {
        Left = std::min(Rhs.X, Left);
        Right = std::max(Rhs.X, Right);
        Top = std::min(Rhs.Y, Top);
        Bottom = std::max(Rhs.Y, Bottom);
        return *this;
    }

    SRect() = default;

    SRect(float Left, float Top, float Right, float Bottom)
        : Left(Left)
        , Top(Top)
        , Right(Right)
        , Bottom(Bottom)
    {
    }

    SRect(const SPoint &Min, const SPoint &Max)
        : Left(Min.X)
        , Top(Min.Y)
        , Right(Max.X)
        , Bottom(Max.Y)
    {
    }

    template <typename TIterator> SRect(TIterator Begin, TIterator End)
    {
        *this = std::accumulate(Begin, End, SRect{}, std::bit_or());
    }

    friend std::ostream &operator<<(std::ostream &Stream, const SRect &Rect)
    {
        Stream << "Left: " << Rect.Left << " Top: " << Rect.Top
               << " Right: " << Rect.Right << " Bottom: " << Rect.Bottom;
        return Stream;
    }
};

struct SGPUUniformData
{
    Rr_Mat4 ViewProjection;
    Rr_IntVec2 ScreenSize;
};

struct SGPUDraw
{
    float X;
    float Y;
    float Width;
    float Height;
    int32_t Type;
    uint32_t Color;
    float Param1;
    float Param2;
};

class CQuadTree
{
private:
    static constexpr uint32_t NULL_NODE = UINT32_MAX;

    struct SNode
    {
        std::array<uint32_t, 4> Indices = {
            NULL_NODE,
            NULL_NODE,
            NULL_NODE,
            NULL_NODE,
        };

        constexpr bool IsEmpty() const
        {
            return std::all_of(
                Indices.cbegin(),
                Indices.cend(),
                [&](auto Index) { return Index == NULL_NODE; });
        }
    };

    SRect Bounds;
    std::vector<SNode> Nodes;
    std::vector<SGPUDraw> Draws;
    uint32_t RootIndex;

    void PushCross(const SPoint &Center, const SRect &Bounds)
    {
        SGPUDraw Draw{};
        Draw.Width = (float)Bounds.Right - (float)Bounds.Left;
        Draw.Height = (float)Bounds.Bottom - (float)Bounds.Top;
        Draw.X = (float)Center.X - Draw.Width / 2.0f;
        Draw.Y = (float)Center.Y - Draw.Height / 2.0f;
        Draw.Type = 1;
        Draws.push_back(Draw);
    }

    void PushPoint(const SPoint &Point, const SRect &Bounds)
    {
        SGPUDraw Draw{};
        const float MaxHor =
            RR_MIN((Bounds.Right - Point.X), (Point.X - Bounds.Left));
        const float MaxVert =
            RR_MIN((Bounds.Bottom - Point.Y), (Point.Y - Bounds.Top));
        Draw.Width = RR_MIN(MaxHor, MaxVert) * 2.0f;
        Draw.Height = Draw.Width;
        Draw.X = (float)Point.X - Draw.Width / 2;
        Draw.Y = (float)Point.Y - Draw.Height / 2;
        Draw.Color = (std::rand() % 256) | ((std::rand() % 256) << 8) |
                     ((std::rand() % 256) << 16);
        Draw.Type = 0;
        Draws.push_back(Draw);
    }

    template <typename TIterator>
    uint32_t BuildNode(const SRect &Bounds, TIterator Begin, TIterator End)
    {
        if(Begin == End)
        {
            return NULL_NODE;
        }

        uint32_t Result = Nodes.size();
        Nodes.emplace_back();

        if(std::equal(Begin + 1, End, Begin))
        {
            PushPoint(*Begin, Bounds);
            return Result;
        }

        if(Begin + 1 == End)
        {
            return Result;
        }

        SPoint Center = Bounds.Center();

        PushCross(Center, Bounds);

        TIterator SplitY =
            std::partition(Begin, End, [Center](const SPoint &Point) {
                return Point.Y < Center.Y;
            });

        TIterator SplitXLower =
            std::partition(Begin, SplitY, [Center](const SPoint &Point) {
                return Point.X < Center.X;
            });

        TIterator SplitXUpper =
            std::partition(SplitY, End, [Center](const SPoint &Point) {
                return Point.X < Center.X;
            });

        Nodes[Result].Indices[0] = BuildNode(
            { { Bounds.Left, Bounds.Top }, Center },
            Begin,
            SplitXLower);

        Nodes[Result].Indices[1] = BuildNode(
            { Center.X, Bounds.Top, Bounds.Right, Center.Y },
            SplitXLower,
            SplitY);

        Nodes[Result].Indices[2] = BuildNode(
            { Bounds.Left, Center.Y, Center.X, Bounds.Bottom },
            SplitY,
            SplitXUpper);

        Nodes[Result].Indices[3] = BuildNode(
            { Center, { Bounds.Right, Bounds.Bottom } },
            SplitXUpper,
            End);

        return Result;
    }

    std::optional<std::reference_wrapper<SGPUDraw>> QueryPoint(
        const SPoint &Point,
        uint32_t NodeIndex,
        SRect &Bounds)
    {
        const SNode &Node = Nodes[NodeIndex];

        if(Node.IsEmpty())
        {
            return std::make_optional(std::reference_wrapper(Draws[NodeIndex]));
        }

        SPoint Center = Bounds.Center();
        if(Point.X > Center.X)
        {
            if(Point.Y > Center.Y)
            {
                if(Node.Indices[3] == NULL_NODE)
                {
                    return {};
                }
                Bounds = {
                    Bounds.Left + Bounds.Extent().X,
                    Bounds.Top + Bounds.Extent().Y,
                    Bounds.Right,
                    Bounds.Bottom,
                };
                return QueryPoint(Point, Node.Indices[3], Bounds);
            }
            else
            {
                if(Node.Indices[1] == NULL_NODE)
                {
                    return {};
                }
                Bounds = {
                    Bounds.Left + Bounds.Extent().X,
                    Bounds.Top,
                    Bounds.Right,
                    Bounds.Top + Bounds.Extent().Y,
                };
                return QueryPoint(Point, Node.Indices[1], Bounds);
            }
        }
        else
        {
            if(Point.Y > Center.Y)
            {
                if(Node.Indices[2] == NULL_NODE)
                {
                    return {};
                }
                Bounds = {
                    Bounds.Left,
                    Bounds.Top + Bounds.Extent().Y,
                    Bounds.Left + Bounds.Extent().X,
                    Bounds.Bottom,
                };
                return QueryPoint(Point, Node.Indices[2], Bounds);
            }
            else
            {
                if(Node.Indices[0] == NULL_NODE)
                {
                    return {};
                }
                Bounds = {
                    Bounds.Left,
                    Bounds.Top,
                    Bounds.Left + Bounds.Extent().X,
                    Bounds.Top + Bounds.Extent().Y,
                };
                return QueryPoint(Point, Node.Indices[0], Bounds);
            }
        }
    }

public:
    template <typename TIterator> void Build(TIterator Begin, TIterator End)
    {
        Nodes.clear();
        Draws.clear();
        Bounds = SRect(Begin, End);
        RootIndex = BuildNode(Bounds, Begin, End);
        // std::sort(
        //     Draws.begin(),
        //     Draws.end(),
        //     [](const SGPUDraw &DrawA, const SGPUDraw &DrawB) {
        //         return DrawA.Type < DrawB.Type;
        //     });
    }

    std::optional<std::reference_wrapper<SGPUDraw>> Query(const SPoint &Point)
    {
        SRect Bounds = this->Bounds;
        return QueryPoint(Point, RootIndex, Bounds);
    }

    const std::vector<SGPUDraw> GetDraws()
    {
        return Draws;
    }
};

const uint32_t MAX_DRAWS = 1024;

static Rr_PipelineLayout *Layout;
static Rr_GraphicsPipeline *Pipeline;
static Rr_Buffer *UniformBuffer;
static Rr_Buffer *StorageBuffer;
static Rr_Buffer *StagingBuffer;

static SGPUUniformData UniformData;

static Rr_Mat4 CameraProjection;
static Rr_Mat4 CameraView;
static float CameraZoom = 1.0f;
static Rr_Vec2 CameraPosition;
static CQuadTree Tree;
static bool Dragging;
static Rr_Vec2 DragStartMouse;
static Rr_Vec2 DragStartCamera;

static void RebuildTree()
{
    const int NUM_POINTS = 1024;
    std::vector<SPoint> Points;
    Points.reserve(NUM_POINTS);
    std::generate_n(std::back_inserter(Points), NUM_POINTS, []() {
        return SPoint{ float(((int)std::rand() % 5000) - 2500),
                       float(((int)std::rand() % 5000) - 2500) };
    });
    Tree.Build(Points.begin(), Points.end());
}

static SPoint ConvertMousePosition(Rr_App *App)
{
    Rr_Vec2 MousePosition = Rr_GetMousePosition(App);
    Rr_IntVec2 WindowSize = Rr_GetWindowSize(App);
    MousePosition.X /= (float)WindowSize.X;
    MousePosition.Y /= (float)WindowSize.Y;
    MousePosition *= 2.0f;
    MousePosition -= Rr_Vec2{ 1.0f, 1.0f };
    Rr_Vec4 Deprojected =
        Rr_InvGeneralM4(CameraProjection) *
        Rr_Vec4{ MousePosition.X, MousePosition.Y, 0.0f, 1.0f };
    Deprojected = Rr_InvGeneralM4(CameraView) * Deprojected;
    return { Deprojected.X, Deprojected.Y };
}

static void Init(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    std::array Bindings = {
        Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
    };
    std::array BindingSets = {
        Rr_PipelineBindingSet{
            Bindings.size(),
            Bindings.data(),
            RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    Layout = Rr_CreatePipelineLayout(
        Renderer,
        BindingSets.size(),
        BindingSets.data());

    Rr_ColorTargetInfo ColorTargets[1] = { 0 };
    ColorTargets[0].Format = RR_TEXTURE_FORMAT_R8G8B8A8_UNORM;
    ColorTargets[0].Blend.BlendEnable = true;
    ColorTargets[0].Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    ColorTargets[0].Blend.DstColorBlendFactor =
        RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorTargets[0].Blend.ColorBlendOp = RR_BLEND_OP_ADD;
    ColorTargets[0].Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    ColorTargets[0].Blend.DstAlphaBlendFactor =
        RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorTargets[0].Blend.AlphaBlendOp = RR_BLEND_OP_ADD;

    Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
    PipelineInfo.Layout = Layout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_FRAG_SPV);
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    Pipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(SGPUUniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT);
    StorageBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(SGPUDraw) * MAX_DRAWS,
        RR_BUFFER_FLAGS_STORAGE_BIT);
    StagingBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(64),
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    // std::srand((uint32_t)std::time(nullptr));

    RebuildTree();
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

    if(Rr_IsScancodePressed(RR_SCANCODE_SPACE))
    {
        RebuildTree();
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_Q))
    {
        CameraZoom += 0.005;
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_E))
    {
        CameraZoom -= 0.005;
    }
    CameraZoom = RR_CLAMP(0.1f, CameraZoom, 10.0f);
    Rr_MouseButtonMask MouseState = Rr_GetMouseState();
    if(Dragging == false && RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_RIGHT_MASK))
    {
        Dragging = true;
        DragStartCamera = CameraPosition;
        DragStartMouse = Rr_GetMousePosition(App);
    }
    if(Dragging == true &&
       RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_RIGHT_MASK) == false)
    {
        Dragging = false;
    }
    if(Dragging)
    {
        CameraPosition =
            DragStartCamera -
            (DragStartMouse - Rr_GetMousePosition(App)) * CameraZoom;
    }
    if(RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_LEFT_MASK) == true)
    {
        SPoint Point = ConvertMousePosition(App);
        auto Result = Tree.Query(Point);
        if(Result.has_value())
        {
            Result.value().get().Color = 0xFFFFFFFF;
        }
    }

    Rr_ColorTarget ColorTarget;
    ColorTarget.Clear = { 1.0, 1.0, 1.0, 1.0 };
    ColorTarget.LoadOp = RR_LOAD_OP_CLEAR;
    ColorTarget.Slot = 0;
    ColorTarget.StoreOp = RR_STORE_OP_STORE;

    const float Left = -SwapchainSize.X / 2.0f * CameraZoom;
    const float Right = SwapchainSize.X / 2.0f * CameraZoom;
    const float Bottom = -SwapchainSize.Y / 2.0f * CameraZoom;
    const float Top = SwapchainSize.Y / 2.0f * CameraZoom;
    CameraProjection = Rr_Orthographic_RH_ZO(Left, Right, Bottom, Top, -1, 1);
    CameraView = Rr_Translate({ CameraPosition.X, CameraPosition.Y, 0.0f });
    UniformData.ViewProjection = CameraProjection * CameraView;
    UniformData.ScreenSize = { SwapchainSize.X, SwapchainSize.Y };

    char *StagingData = (char *)Rr_GetMappedBufferData(Renderer, StagingBuffer);
    std::memcpy(StagingData, &UniformData, sizeof(UniformData));
    StagingData += sizeof(UniformData);
    std::uint32_t DrawsSize = sizeof(SGPUDraw) * Tree.GetDraws().size();
    std::memcpy(StagingData, Tree.GetDraws().data(), DrawsSize);

    Rr_GraphNode *TransferNode = Rr_AddTransferNode(Renderer, "transfer");
    Rr_TransferBufferData(
        TransferNode,
        sizeof(UniformData),
        StagingBuffer,
        0,
        UniformBuffer,
        0);
    Rr_TransferBufferData(
        TransferNode,
        DrawsSize,
        StagingBuffer,
        sizeof(UniformData),
        StorageBuffer,
        0);

    Rr_GraphNode *TreeNode = Rr_AddGraphicsNode(
        Renderer,
        "tree",
        1,
        &ColorTarget,
        &SwapchainImage,
        nullptr,
        nullptr);
    Rr_BindGraphicsPipeline(TreeNode, Pipeline);
    Rr_BindUniformBuffer(TreeNode, UniformBuffer, 0, 0, 0, sizeof(UniformData));
    Rr_BindStorageBuffer(TreeNode, StorageBuffer, 0, 1, 0, DrawsSize);
    Rr_Draw(TreeNode, 6, Tree.GetDraws().size(), 0, 0);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_DestroyBuffer(Renderer, UniformBuffer);
    Rr_DestroyBuffer(Renderer, StorageBuffer);
    Rr_DestroyBuffer(Renderer, StagingBuffer);
    Rr_DestroyGraphicsPipeline(Renderer, Pipeline);
    Rr_DestroyPipelineLayout(Renderer, Layout);
}

int main()
{
    Rr_AppConfig Config = {};
    Config.Title = "08_QuadTree";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.08_quadtree";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
