#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>

struct SPoint
{
    int32_t X, Y;

    bool operator==(const SPoint &Rhs) const
    {
        return X == Rhs.X && Y == Rhs.Y;
    }
};

struct SRect
{
    int32_t Left, Top, Right, Bottom;

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

    friend std::ostream &operator<<(std::ostream &Stream, const SRect &Rect)
    {
        Stream << "Left: " << Rect.Left << " Top: " << Rect.Top
               << " Right: " << Rect.Right << " Bottom: " << Rect.Bottom;
        return Stream;
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

    SRect(int32_t Left, int32_t Top, int32_t Right, int32_t Bottom)
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
};

template <typename TItem> class CQuadTree
{
public:
    struct SNode
    {
        uint32_t Indices[2][2] = {
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX,
            UINT32_MAX,
        };
    };

    SRect Bounds;
    std::vector<SNode> Nodes;
    std::vector<TItem> Items;
    uint32_t RootIndex;

    template <typename TIterator>
    uint32_t Build(const SRect &Bounds, TIterator Begin, TIterator End)
    {
        if(Begin == End)
        {
            return UINT32_MAX;
        }

        uint32_t Result = Nodes.size();
        Nodes.emplace_back();

        if(std::equal(Begin + 1, End, Begin))
        {
            return Result;
        }

        if(Begin + 1 == End)
        {
            return Result;
        }

        SPoint Center = Bounds.Center();

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

        Nodes[Result].Indices[0][0] =
            Build({ { Bounds.Left, Bounds.Top }, Center }, Begin, SplitXLower);

        Nodes[Result].Indices[0][1] = Build(
            { Center.X, Bounds.Top, Bounds.Right, Center.Y },
            SplitXLower,
            SplitY);

        Nodes[Result].Indices[1][0] = Build(
            { Bounds.Left, Center.Y, Center.X, Bounds.Bottom },
            SplitY,
            SplitXUpper);

        Nodes[Result].Indices[1][1] = Build(
            { Center, { Bounds.Right, Bounds.Bottom } },
            SplitXUpper,
            End);

        return Result;
    }

public:
    template <typename TIterator>
    CQuadTree(TIterator Begin, TIterator End)
        : Bounds(SRect{ Begin, End })
    {
        RootIndex = Build(Bounds, Begin, End);
    }
};

struct SGPUUniformData
{
    Rr_Mat4 View;
    Rr_Mat4 Projection;
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

const uint32_t MAX_DRAWS = 1024;

static Rr_PipelineLayout *Layout;
static Rr_GraphicsPipeline *Pipeline;
static Rr_Buffer *UniformBuffer;
static Rr_Buffer *StorageBuffer;
static Rr_Buffer *StagingBuffer;

static SGPUUniformData UniformData;
static std::vector<SGPUDraw> Draws;

static float CameraZoom = 1.0f;
static Rr_Vec2 CameraPosition;

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

    for(auto Index = 0; Index < 14; ++Index)
    {
        SGPUDraw Draw{};
        Draw.X =
            (((float)std::rand() / (float)RAND_MAX) - 0.5f) * 2.0f * 256.0f;
        Draw.Y =
            (((float)std::rand() / (float)RAND_MAX) - 0.5f) * 2.0f * 256.0f;
        float Radius = (float)std::rand() / (float)RAND_MAX * 256.0f + 10.0f;
        Draw.Width = Radius;
        Draw.Height = Radius;
        Draw.Color = (std::rand() % 256) | ((std::rand() % 256) << 8) |
                     ((std::rand() % 256) << 16);
        Draws.push_back(Draw);
    }
}

static void Iterate(Rr_App *App, void *UserData)
{
    if(Rr_IsScancodePressed(RR_SCANCODE_Q))
    {
        CameraZoom -= 0.005;
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_E))
    {
        CameraZoom += 0.005;
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_A))
    {
        CameraPosition.X += 0.5;
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_D))
    {
        CameraPosition.X -= 0.5;
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_W))
    {
        CameraPosition.Y += 0.5;
    }
    if(Rr_IsScancodePressed(RR_SCANCODE_S))
    {
        CameraPosition.Y -= 0.5;
    }

    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

    Rr_ColorTarget ColorTarget;
    ColorTarget.Clear = { 1.0, 1.0, 1.0, 1.0 };
    ColorTarget.LoadOp = RR_LOAD_OP_CLEAR;
    ColorTarget.Slot = 0;
    ColorTarget.StoreOp = RR_STORE_OP_STORE;

    // Rr_GraphNode *ClearNode = Rr_AddGraphicsNode(
    //     Renderer,
    //     "clear",
    //     1,
    //     &ColorTarget,
    //     &SwapchainImage,
    //     nullptr,
    //     nullptr);

    UniformData.Projection = Rr_Orthographic_RH_ZO(
        -SwapchainSize.X / 2.0f * CameraZoom,
        SwapchainSize.X / 2.0f * CameraZoom,
        -SwapchainSize.Y / 2.0f * CameraZoom,
        SwapchainSize.Y / 2.0f * CameraZoom,
        -1,
        1);
    UniformData.View =
        Rr_Translate({ CameraPosition.X, CameraPosition.Y, 0.0f });
    UniformData.ScreenSize = { SwapchainSize.X, SwapchainSize.Y };

    char *StagingData = (char *)Rr_GetMappedBufferData(Renderer, StagingBuffer);
    std::memcpy(StagingData, &UniformData, sizeof(UniformData));
    StagingData += sizeof(UniformData);
    std::uint32_t DrawsSize = sizeof(SGPUDraw) * Draws.size();
    std::memcpy(StagingData, Draws.data(), DrawsSize);

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
    Rr_Draw(TreeNode, 6, Draws.size(), 0, 0);
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
    std::srand((uint32_t)std::time(nullptr));

    // CQuadTree<uint32_t> Tree;
    SRect RectA{ 10, 10, 20, 20 };
    SRect RectB{ 11, 11, 19, 19 };
    std::cout << RectA << "\n" << RectB << "\n";
    std::cout << "Intersects: " << RectA.Intersects(RectB) << std::endl;
    std::cout << "Contains: " << RectA.Contains(RectB) << std::endl;

    std::vector<SPoint> Points = {
        { -1, 1 },
        { 5, 0 },
        { 4, 10 },
        { -5, 4 },
    };
    std::cout << "BoundingBox: " << SRect(Points.begin(), Points.end())
              << std::endl;

    auto Split =
        std::partition(Points.begin(), Points.end(), [](const SPoint &Point) {
            return Point.X > 0;
        });

    for(auto It = Split; It != Points.end(); ++It)
    {
        std::cout << It->X << " " << It->Y << "\n";
    }

    {
        const int NUM_POINTS = 32;
        std::vector<SPoint> Points;
        Points.reserve(NUM_POINTS);
        std::generate_n(std::back_inserter(Points), NUM_POINTS, []() {
            return SPoint{ ((int)std::rand() % 256) - 128,
                           ((int)std::rand() % 256) - 128 };
        });
        // std::generate_n
        for(auto &Point : Points)
        {
            std::cout << Point.X << " " << Point.Y << "\n";
        }
        CQuadTree<double> Tree{ Points.begin(), Points.end() };

        std::cout << Tree.Bounds << "\n";
        std::cout << Tree.Nodes.size() << "\n";
    }

    Rr_AppConfig Config = {};
    Config.Title = "08_QuadTree";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.08_quadtree";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
