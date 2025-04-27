#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <unordered_set>
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

    constexpr SPoint Center() const
    {
        return { (Right + Left) / 2, (Top + Bottom) / 2 };
    }

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

    constexpr bool IntersectsCircle(const SPoint &Center, float Radius) const
    {
        float ClosestX = RR_CLAMP(Left, Center.X, Right);
        float ClosestY = RR_CLAMP(Top, Center.Y, Bottom);
        float DistanceX = Center.X - ClosestX;
        float DistanceY = Center.Y - ClosestY;
        float DistanceSquared =
            (DistanceX * DistanceX) + (DistanceY * DistanceY);
        return DistanceSquared < (Radius * Radius);
    }

    const SRect Clamp(const SRect &Rhs) const
    {
        return {
            std::max(Left, Rhs.Left),
            std::max(Top, Rhs.Top),
            std::min(Right, Rhs.Right),
            std::min(Bottom, Rhs.Bottom),
        };
    }

    const SRect LeftTop() const
    {
        return { Left, Top, Left + Extent().X, Top + Extent().Y };
    }

    const SRect RightTop() const
    {
        return { Left + Extent().X, Top, Right, Top + Extent().Y };
    }

    const SRect LeftBottom() const
    {
        return { Left, Top + Extent().Y, Left + Extent().X, Bottom };
    }

    const SRect RightBottom() const
    {
        return { Left + Extent().X, Top + Extent().Y, Right, Bottom };
    }

    const std::array<SRect, 4> Quadrants() const
    {
        return { LeftTop(), RightTop(), LeftBottom(), RightBottom() };
    }

    const SPoint Extent() const
    {
        return { std::abs(Left - Right) / 2, std::abs(Bottom - Top) / 2 };
    }

    const SPoint Size() const
    {
        return { std::abs(Left - Right), std::abs(Bottom - Top) };
    }

    const SRect Quad() const
    {
        float Min = std::min(Left, std::min(Right, std::min(Top, Bottom)));
        float Max = std::max(Left, std::max(Right, std::max(Top, Bottom)));
        return { Min, Min, Max, Max };
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
        : Left(std::min(Left, Right))
        , Top(std::min(Top, Bottom))
        , Right(std::max(Left, Right))
        , Bottom(std::max(Top, Bottom))
    {
    }

    SRect(const SPoint &Min, const SPoint &Max)
        : SRect(Min.X, Min.Y, Max.X, Max.Y)
    {
    }

    template<typename TIterator> SRect(TIterator Begin, TIterator End)
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
    float Time;
};

enum class EDrawType : uint32_t
{
    CIRCLE,
    CROSS,
    RECT_SELECTION,
    RECT_TREE_BORDER,
};

struct SGPUDraw
{
    float X;
    float Y;
    float Width;
    float Height;
    EDrawType Type;
    uint32_t Color;
    float Param1;
    float Param2;

    SRect Bounds() const
    {
        return { X, Y, X + Width, Y + Height };
    }

    friend SRect &operator|(SRect &Rect, const SGPUDraw &Rhs)
    {
        Rect = Rect | SPoint{ Rhs.X, Rhs.Y };
        Rect = Rect | SPoint{ Rhs.X + Rhs.Width, Rhs.Y + Rhs.Height };
        return Rect;
    }
};

template<
    typename TPayload,
    std::size_t MaxDepth = 12,
    typename =
        typename std::enable_if_t<std::is_copy_constructible_v<TPayload>>>
class CQuadTree
{
private:
    using UIndex = std::uint32_t;

    static constexpr UIndex NULL_NODE = std::numeric_limits<UIndex>::max();

    struct SNode
    {
        UIndex First{ NULL_NODE };
        std::int32_t Count{};
    };

    struct SElement
    {
        TPayload Payload;
        SRect Bounds;
    };

    struct SElementNode
    {
        UIndex ElementIndex;
        UIndex Next = NULL_NODE;
    };

    SRect Bounds;
    uint32_t RootIndex;
    std::vector<SNode> Nodes;
    std::vector<SElement> Elements;
    std::vector<SElementNode> ElementNodes;
    UIndex NextElementNodeIndex = NULL_NODE;

    void QueryRect(
        const SRect &Rect,
        uint32_t NodeIndex,
        SRect &Bounds,
        std::unordered_set<TPayload> &Result) const
    {
        if(Nodes.empty())
        {
            return;
        }

        const SNode &Node = Nodes[NodeIndex];

        if(Node.Count != -1)
        {
            UIndex ElementNodeIndex = Node.First;
            while(ElementNodeIndex != NULL_NODE)
            {
                const auto ElementIndex =
                    ElementNodes[ElementNodeIndex].ElementIndex;
                Result.emplace(Elements[ElementIndex].Payload);
                ElementNodeIndex = ElementNodes[ElementNodeIndex].Next;
            }
            return;
        }

        auto Quadrants = Bounds.Quadrants();
        for(auto Index = 0; Index < 4; ++Index)
        {
            SRect Quadrant = Quadrants[Index];
            if(Rect.Intersects(Quadrant))
            {
                QueryRect(Rect, Node.First + Index, Quadrant, Result);
            }
        }
    }

    void ForEachDebugDrawInRect(
        const SRect &Rect,
        uint32_t NodeIndex,
        SRect &Bounds,
        const std::function<void(const SGPUDraw &)> &Callback)
    {
        if(Nodes.empty())
        {
            return;
        }

        const SNode &Node = Nodes[NodeIndex];

        if(Node.Count != -1)
        {
            return;
        }

        if(Node.Count == -1)
        {
            SGPUDraw Draw{};
            Draw.Width = Bounds.Right - Bounds.Left;
            Draw.Height = Bounds.Bottom - Bounds.Top;
            auto Center = Bounds.Center();
            Draw.X = Center.X - Draw.Width / 2.0f;
            Draw.Y = Center.Y - Draw.Height / 2.0f;
            Draw.Type = EDrawType::CROSS;
            Callback(Draw);
        }

        auto Quadrants = Bounds.Quadrants();
        for(auto Index = 0; Index < 4; ++Index)
        {
            SRect Quadrant = Quadrants[Index];
            if(Rect.Intersects(Quadrant))
            {
                ForEachDebugDrawInRect(
                    Rect,
                    Node.First + Index,
                    Quadrant,
                    Callback);
            }
        }
    }

    UIndex AddFourNodes()
    {
        UIndex Current = (UIndex)Nodes.size();
        for(auto Index = 0; Index < 4; ++Index)
        {
            Nodes.emplace_back();
        }
        return Current;
    }

    void AddElementToNode(SNode &Node, UIndex ElementIndex)
    {
        UIndex ElementNodeIndex;
        if(NextElementNodeIndex != NULL_NODE)
        {
            ElementNodeIndex = NextElementNodeIndex;
            NextElementNodeIndex = ElementNodes[ElementNodeIndex].Next;
            ElementNodes[ElementNodeIndex].ElementIndex = ElementIndex;
            ElementNodes[ElementNodeIndex].Next = Node.First;
        }
        else
        {
            ElementNodeIndex = (UIndex)ElementNodes.size();
            ElementNodes.emplace_back(SElementNode{ ElementIndex, Node.First });
        }
        Node.First = ElementNodeIndex;
        Node.Count++;
    }

    void ConvertToBranch(UIndex NodeIndex, const SRect &Bounds)
    {
        UIndex ElementNodeIndex = Nodes[NodeIndex].First;
        UIndex Current = AddFourNodes();
        Nodes[NodeIndex].First = (UIndex)Current;
        Nodes[NodeIndex].Count = -1;

        /* This will propagate existing elements down
         * to newly created subnodes taking element
         * bounds into account.*/

        auto Quadrants = Bounds.Quadrants();
        while(ElementNodeIndex != NULL_NODE)
        {
            for(UIndex QuadrantIndex = 0; QuadrantIndex < 4; ++QuadrantIndex)
            {
                auto &ElementNode = ElementNodes[ElementNodeIndex];
                if(Quadrants[QuadrantIndex].Intersects(
                       Elements[ElementNode.ElementIndex].Bounds))
                {
                    AddElementToNode(
                        Nodes[Current + QuadrantIndex],
                        ElementNode.ElementIndex);
                }
            }

            auto Next = ElementNodes[ElementNodeIndex].Next;

            /* Release element node to free list. */

            auto &ElementNodeToRecycle = ElementNodes[ElementNodeIndex];
            ElementNodeToRecycle.Next = NextElementNodeIndex;
            NextElementNodeIndex = ElementNodeIndex;

            /* Proceed to next element node. */

            ElementNodeIndex = Next;
        }
    }

    void InsertRecursive(
        UIndex ElementIndex,
        UIndex NodeIndex,
        const SRect &Bounds,
        std::size_t Depth)
    {
        const auto &ElementBounds = Elements[ElementIndex].Bounds;
        if(Nodes[NodeIndex].Count == 0 || Depth == 0) /* It's a leaf node. */
        {
            AddElementToNode(Nodes[NodeIndex], ElementIndex);
            return;
        }
        else if(Nodes[NodeIndex].Count > 0) /* Split the node. */
        {
            ConvertToBranch(NodeIndex, Bounds);
        }

        /* It's a branch node. */

        auto Quadrants = Bounds.Quadrants();
        for(UIndex Index = 0; Index < 4; ++Index)
        {
            if(Quadrants[Index].Intersects(ElementBounds))
            {
                InsertRecursive(
                    ElementIndex,
                    Nodes[NodeIndex].First + Index,
                    Quadrants[Index],
                    Depth - 1);
            }
        }
    }

public:
    CQuadTree &operator=(CQuadTree &&Rhs)
    {
        Bounds = Rhs.Bounds;
        RootIndex = Rhs.RootIndex;
        Nodes = std::move(Rhs.Nodes);
        Elements = std::move(Rhs.Elements);
        ElementNodes = std::move(Rhs.ElementNodes);
        NextElementNodeIndex = Rhs.NextElementNodeIndex;
        return *this;
    }

    bool Insert(const TPayload &Payload, const SRect &ElementBounds)
    {
        if(!Bounds.Contains(ElementBounds))
        {
            return false;
        }

        UIndex ElementIndex = (UIndex)Elements.size();
        Elements.emplace_back(SElement{ Payload, ElementBounds });

        InsertRecursive(ElementIndex, RootIndex, Bounds, MaxDepth);

        return true;
    }

    void Reset(const SRect &Bounds)
    {
        this->Bounds = Bounds.Quad();
        Nodes.clear();
        Elements.clear();
        ElementNodes.clear();
        Nodes.emplace_back();
        RootIndex = 0;
    }

    void Query(const SRect &Rect, std::unordered_set<TPayload> &Result) const
    {
        SRect Bounds = this->Bounds;
        if(Bounds.Intersects(Rect))
        {
            QueryRect(Bounds.Clamp(Rect), RootIndex, Bounds, Result);
        }
    }

    void ForEachDebugDraw(
        const SRect &Rect,
        const std::function<void(const SGPUDraw &)> &Callback)
    {
        SRect Bounds = this->Bounds;
        if(Bounds.Intersects(Rect))
        {
            ForEachDebugDrawInRect(
                Bounds.Clamp(Rect),
                RootIndex,
                Bounds,
                Callback);
        }

        SGPUDraw TreeBorder{};
        TreeBorder.Width = Bounds.Size().X;
        TreeBorder.Height = Bounds.Size().Y;
        TreeBorder.X = Bounds.Left;
        TreeBorder.Y = Bounds.Top;
        TreeBorder.Type = EDrawType::RECT_TREE_BORDER;
        Callback(TreeBorder);
    }

    size_t ElementsCount() const
    {
        return Elements.size();
    }
};

const uint32_t MAX_DRAWS = 1 << 14;

static Rr_PipelineLayout *Layout;
static Rr_GraphicsPipeline *Pipeline;
static Rr_Buffer *UniformBuffer;
static Rr_Buffer *StorageBuffer;
static Rr_Buffer *StagingBuffer;

static SGPUUniformData UniformData;

static CQuadTree<std::size_t> Tree;
static std::vector<SGPUDraw> Draws;

static Rr_Mat4 CameraProjection;
static Rr_Mat4 CameraView;
static float CameraZoom = 1.0f;
static Rr_Vec2 CameraPosition;
static std::unordered_set<std::size_t> RenderResult;

static bool Dragging;
static Rr_Vec2 DragStartMouse;
static Rr_Vec2 DragStartCamera;

static bool Selecting;
static bool TrySelect;
static SPoint SelectStart;
static SPoint SelectEnd;
static std::unordered_set<std::size_t> SelectResult;

static bool DrawDebug = true;
static bool UseQuery = true;
static bool VSyncEnabled = true;
static std::default_random_engine RandomEngine;

static std::size_t DrawCount = 0;
static std::size_t DrawsSize = 0;

std::mutex Mutex;
bool Rebuilding;
std::mutex RebuildMutex;

static float GetRandomFloat(float Min, float Max)
{
    std::uniform_real_distribution<double> Distribution(Min, Max);

    return Distribution(RandomEngine);
}

static uint32_t GetRandomColor()
{
    static std::uniform_int_distribution<std::uint32_t> Distribution(
        0,
        std::numeric_limits<unsigned char>::max());

    std::uint32_t Color = (Distribution(RandomEngine) << 0) |
                          (Distribution(RandomEngine) << 8) |
                          (Distribution(RandomEngine) << 16);
    return Color;
}

static void RebuildTree()
{
    if(auto RebuildLock = std::unique_lock(RebuildMutex, std::try_to_lock))
    {
        auto Thread = std::thread([&]() {
            auto RebuildLock = std::lock_guard(RebuildMutex);

            Rebuilding = true;

            CQuadTree<std::size_t> NewTree;
            std::vector<SGPUDraw> NewDraws;

            const uint32_t NUM_POINTS = 400000;
            const float AREA_WIDTH = 80000.0f;
            const float AREA_HEIGHT = 50000.0f;
            NewDraws.reserve(NUM_POINTS);
            NewDraws.clear();
            NewTree.Reset(
                { -AREA_WIDTH, -AREA_HEIGHT, AREA_WIDTH, AREA_HEIGHT });
            for(auto Index = 0; Index < NUM_POINTS; ++Index)
            {
                SGPUDraw Draw{};
                Draw.Width = GetRandomFloat(96.0f, 128.0f);
                Draw.Height = Draw.Width;
                Draw.X = GetRandomFloat(-AREA_WIDTH, AREA_WIDTH);
                Draw.Y = GetRandomFloat(-AREA_HEIGHT, AREA_HEIGHT);
                Draw.Color = GetRandomColor();
                Draw.Type = EDrawType::CIRCLE;

                /* Some points may not be eligble for the tree.
                 * It's important to use Draws.size() as index
                 * because "Index" iterator doesn't refer
                 * to real index in the vector if some points
                 * are being skipped. */

                auto ElementIndex = NewDraws.size();
                if(NewTree.Insert(ElementIndex, Draw.Bounds()))
                {
                    NewDraws.emplace_back(Draw);
                }
            }

            auto Lock = std::lock_guard(Mutex);
            Tree = std::move(NewTree);
            Draws = std::move(NewDraws);

            Rebuilding = false;
        });
        Thread.detach();
    }
}

static SRect GetScreenRect()
{
    Rr_Vec4 DeprojectedMin =
        Rr_InvGeneralM4(CameraProjection) * Rr_Vec4{ -1.0f, -1.0f, 0.0f, 1.0f };
    DeprojectedMin = Rr_InvGeneralM4(CameraView) * DeprojectedMin;

    Rr_Vec4 DeprojectedMax =
        Rr_InvGeneralM4(CameraProjection) * Rr_Vec4{ 1.0f, 1.0f, 0.0f, 1.0f };
    DeprojectedMax = Rr_InvGeneralM4(CameraView) * DeprojectedMax;
    return { DeprojectedMin.X,
             DeprojectedMin.Y,
             DeprojectedMax.X,
             DeprojectedMax.Y };
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
    ColorTargets[0].Format = Rr_GetSwapchainFormat(Renderer);
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

    RebuildTree();
}

static void Event(Rr_App *App, Rr_Event *Event)
{
    switch(Event->Type)
    {
        case RR_EVENT_TYPE_MOUSE_WHEEL:
        {
            if(Rr_WantMouseCapture())
            {
                return;
            }

            CameraZoom += Event->Wheel.Amount.Y * -0.5f;
            CameraZoom = RR_CLAMP(0.1f, CameraZoom, 100.0f);
        }
        break;
        case RR_EVENT_TYPE_MOUSE_MOTION:
        {
            if(TrySelect)
            {
                SelectEnd = ConvertMousePosition(App);
                Selecting = std::fabs(SelectStart.X - SelectEnd.X) > 10.0f ||
                            std::fabs(SelectStart.Y - SelectEnd.Y) > 10.0f;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_DOWN:
        {
            if(Rr_WantMouseCapture())
            {
                return;
            }

            if(Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                SelectStart = ConvertMousePosition(App);
                TrySelect = true;
                Selecting = false;
            }
            else if(Event->MouseButton.Button == RR_MOUSE_BUTTON_RIGHT)
            {
                Dragging = true;
                DragStartCamera = CameraPosition;
                DragStartMouse = Rr_GetMousePosition(App);
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if(Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                if(Selecting == false && Rr_WantMouseCapture() == false)
                {
                    if(auto Lock = std::unique_lock(Mutex, std::try_to_lock))
                    {
                        SPoint Point = ConvertMousePosition(App);
                        SGPUDraw Draw{};
                        Draw.Width = GetRandomFloat(32.0f, 64.0f);
                        Draw.Height = Draw.Width;
                        Draw.X = Point.X - Draw.Width / 2.0f;
                        Draw.Y = Point.Y - Draw.Height / 2.0f;
                        Draw.Color = GetRandomColor();
                        if(Tree.Insert(Draws.size(), Draw.Bounds()))
                        {
                            Draws.emplace_back(Draw);
                        }
                    }
                }
                TrySelect = false;
                Selecting = false;
            }
            else if(Event->MouseButton.Button == RR_MOUSE_BUTTON_RIGHT)
            {
                Dragging = false;
            }
        }
        break;
        default:
            break;
    }
}

static void Update(Rr_App *App)
{
    float DeltaTime = Rr_GetDeltaSeconds(App);

    Rr_MouseButtonFlags MouseState = Rr_GetMouseState();

    if(Dragging)
    {
        CameraPosition =
            DragStartCamera -
            (DragStartMouse - Rr_GetMousePosition(App)) * CameraZoom;
    }

    if(auto Lock = std::unique_lock(Mutex, std::try_to_lock))
    {
        if(Rr_IsScancodePressed(RR_SCANCODE_SPACE))
        {
            RebuildTree();
        }

        if(Selecting)
        {
            const SRect QueryRect{ SelectStart, SelectEnd };
            if(UseQuery)
            {
                Tree.Query(QueryRect, SelectResult);
                for(auto Index : SelectResult)
                {
                    auto Bounds = Draws[Index].Bounds();
                    auto Center = Bounds.Center();
                    auto Radius = Bounds.Extent().X;
                    if(QueryRect.IntersectsCircle(Center, Radius))
                    {
                        Draws[Index].Param1 = 1.0f;
                    }
                }
            }
            else
            {
                for(auto Index = 0; Index < Draws.size(); ++Index)
                {
                    auto Bounds = Draws[Index].Bounds();
                    auto Center = Bounds.Center();
                    auto Radius = Bounds.Extent().X;
                    if(QueryRect.IntersectsCircle(Center, Radius))
                    {
                        Draws[Index].Param1 = 1.0f;
                        SelectResult.emplace(Index);
                    }
                }
            }
        }
    }
}

static void Render(Rr_App *App)
{
    auto Lock = std::unique_lock(Mutex, std::try_to_lock);

    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

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
    UniformData.Time = Rr_GetTimeSeconds(App);

    char *StagingDataStart =
        (char *)Rr_GetMappedBufferData(Renderer, StagingBuffer);
    char *StagingData = StagingDataStart;
    std::memcpy(StagingData, &UniformData, sizeof(UniformData));
    StagingData += sizeof(UniformData);

    /* Query screen rect and populate draws. */

    DrawCount = DrawsSize = 0;

    if(Lock.owns_lock())
    {
        SRect ScreenRect = GetScreenRect();
        Tree.Query(ScreenRect, RenderResult);
        if(RenderResult.size() > 0)
        {
            DrawCount += RenderResult.size();
            for(auto Index : RenderResult)
            {
                std::memcpy(StagingData, &Draws[Index], sizeof(SGPUDraw));
                StagingData += sizeof(SGPUDraw);
            }
        }

        if(Selecting)
        {
            SRect SelectRect = { SelectStart, SelectEnd };
            SGPUDraw Draw{};
            Draw.X = SelectRect.Left;
            Draw.Y = SelectRect.Top;
            Draw.Width = SelectRect.Size().X;
            Draw.Height = SelectRect.Size().Y;
            Draw.Type = EDrawType::RECT_SELECTION;
            Draw.Color = 0xffecc5ad;
            std::memcpy(StagingData, &Draw, sizeof(SGPUDraw));
            StagingData += sizeof(SGPUDraw);
            DrawCount++;
        }

        if(DrawDebug)
        {
            Tree.ForEachDebugDraw(ScreenRect, [&](const SGPUDraw &Draw) {
                std::memcpy(StagingData, &Draw, sizeof(SGPUDraw));
                StagingData += sizeof(SGPUDraw);
                DrawCount++;
            });
        }

        DrawsSize = RR_MIN(
            StagingData - StagingDataStart - sizeof(SGPUUniformData),
            MAX_DRAWS * sizeof(SGPUDraw));
        DrawCount = RR_MIN(DrawCount, MAX_DRAWS);
    }

    if(DrawCount > 0)
    {
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
    }

    Rr_GraphNode *TreeNode = Rr_AddGraphicsNode(
        Renderer,
        "tree",
        1,
        &ColorTarget,
        &SwapchainImage,
        nullptr,
        nullptr);
    Rr_BindGraphicsPipeline(TreeNode, Pipeline);
    if(DrawCount > 0)
    {
        Rr_BindUniformBuffer(
            TreeNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(UniformData));
        Rr_BindStorageBuffer(TreeNode, StorageBuffer, 0, 1, 0, DrawsSize);
        Rr_Draw(TreeNode, 6, DrawCount, 0, 0);
    }
}

static void Reset()
{
    for(auto Index : SelectResult)
    {
        Draws[Index].Param1 = 0.0f;
    }
    SelectResult.clear();
    RenderResult.clear();
}

static void Iterate(Rr_App *App, void *UserData)
{
    static float TimeCounter = 1.0f;
    static float FPSCount;
    TimeCounter += Rr_GetDeltaSeconds(App);
    if(TimeCounter > 0.2f)
    {
        TimeCounter = 0.0f;
        FPSCount = Rr_GetFramesPerSecond(App);
    }

    Rr_BeginWindow("QuadTree", 0);
    Rr_LabelF("FPS: %.2f", FPSCount);
    Rr_LabelF("Circles: %zu", Tree.ElementsCount());
    Rr_LabelF("Rebuilding: %d", Rebuilding);
    Rr_BeginHorizontal();
    if(Rr_Button("Regenerate Tree"))
    {
        if(auto Lock = std::unique_lock(Mutex, std::try_to_lock))
        {
            RebuildTree();
        }
    }
    if(Rr_Button("Test"))
    {
    }
    Rr_EndHorizontal();
    Rr_LabelF("Draw Count: %d", DrawCount);
    Rr_LabelF("Draws Size: %d", DrawsSize);
    Rr_LabelF("Box Select: %d", Selecting);
    Rr_LabelF(
        "Camera Position: %d %d",
        (int)CameraPosition.X,
        (int)CameraPosition.Y);
    Rr_Separator();
    Rr_Checkbox("Debug Draw", &DrawDebug);
    Rr_BeginHorizontal();
    Rr_Checkbox("Use Query", &UseQuery);
    if(Rr_Checkbox("Use VSync", &VSyncEnabled))
    {
        Rr_SetSwapchainPresentMode(
            Rr_GetRenderer(App),
            VSyncEnabled ? RR_PRESENT_MODE_FIFO : RR_PRESENT_MODE_IMMEDIATE);
    }
    Rr_EndHorizontal();
    Rr_Separator();
    Rr_Label("Sample Wrapped Text Sample Wrapped Text Sample Wrapped Text "
             "Sample Wrapped Text Sample Wrapped Text Sample Wrapped Text");
    Rr_Separator();
    Rr_Label("Multi\n line\n  text");
    Rr_EndWindow();

    Update(App);
    Render(App);
    Reset();
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
    Config.EventFunc = Event;
    Config.IterateFunc = Iterate;
    Config.CleanupFunc = Cleanup;
    Rr_Run(&Config);
}
