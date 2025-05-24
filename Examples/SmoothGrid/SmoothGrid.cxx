#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cfloat>
#include <vector>

struct SCamera
{
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{};

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    void SetPerspective(
        float FOVDegrees,
        Rr_IntVec2 Size,
        float Near,
        float Far)
    {
        ProjMatrix = Rr_Perspective_LH_ZO(
            RR_ANGLE_DEG(FOVDegrees),
            (float)Size.X / (float)Size.Y,
            Near,
            Far);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void Update(Rr_InputState *State)
    {
        float DeltaTime = Rr_GetDeltaSeconds();

        // Rr_KeyStates Keys = State->Keys;

        // Rr_Vec3 CameraForward = GetForwardVector();
        // Rr_Vec3 CameraLeft = GetRightVector();
        // constexpr float CameraSpeed = 5.0f;
        // if(Rr_GetKeyState(Keys, EIA_UP) == RR_KEYSTATE_HELD)
        // {
        //     Position += CameraForward * CameraSpeed * DeltaTime;
        // }
        // if(Rr_GetKeyState(Keys, EIA_LEFT) == RR_KEYSTATE_HELD)
        // {
        //     Position -= CameraLeft * CameraSpeed * DeltaTime;
        // }
        // if(Rr_GetKeyState(Keys, EIA_DOWN) == RR_KEYSTATE_HELD)
        // {
        //     Position -= CameraForward * CameraSpeed * DeltaTime;
        // }
        // if(Rr_GetKeyState(Keys, EIA_RIGHT) == RR_KEYSTATE_HELD)
        // {
        //     Position += CameraLeft * CameraSpeed * DeltaTime;
        // }

        // if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_BIT)
        // {
        //     Rr_SetRelativeMouseMode(true);
        //     constexpr float Sensitivity = 0.2f;
        //     Yaw = Rr_WrapMax(
        //         Yaw + (State->MousePositionDelta.X * Sensitivity),
        //         360.0f);
        //     Pitch = Rr_WrapMinMax(
        //         Pitch - (State->MousePositionDelta.Y * Sensitivity),
        //         -90.0f,
        //         90.0f);
        // }
        // else
        // {
        //     Rr_SetRelativeMouseMode(false);
        // }

        // float CosPitch = cosf(Pitch * RR_DEG_TO_RAD);
        // float SinPitch = sinf(Pitch * RR_DEG_TO_RAD);
        // float CosYaw = cosf(Yaw * RR_DEG_TO_RAD);
        // float SinYaw = sinf(Yaw * RR_DEG_TO_RAD);

        // Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
        // Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
        // Rr_Vec3 ZAxis{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };

        // ViewMatrix = {
        //     XAxis.X,
        //     YAxis.X,
        //     ZAxis.X,
        //     0.0f,
        //     XAxis.Y,
        //     YAxis.Y,
        //     ZAxis.Y,
        //     0.0f,
        //     XAxis.Z,
        //     YAxis.Z,
        //     ZAxis.Z,
        //     0.0f,
        //     -Rr_Dot(XAxis, Position),
        //     -Rr_Dot(YAxis, Position),
        //     -Rr_Dot(ZAxis, Position),
        //     1.0f,
        // };
    }
};

struct SSmoothGrid
{
    SCamera Camera;
    Rr_Renderer *Renderer;

    SSmoothGrid()
        : Renderer(Rr_GetRenderer())
    {
        Camera.Position = { 0.0f, -0.5f, -2.5f };
    }

    void Event()
    {

    }

    void Iterate()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
        Camera.SetPerspective(50.0f, SwapchainSize, 0.1f, 200.0f);
        // Camera.Update(&InputState);

        Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    }

    ~SSmoothGrid()
    {
    }
};

static void Init(void *UserData)
{
    new (UserData) SSmoothGrid();
}

static void Event(void *UserData, Rr_Event *Event)
{
    auto SmoothGrid = std::bit_cast<SSmoothGrid *>(UserData);
    SmoothGrid->Event();
}

static void Iterate(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SSmoothGrid *>(UserData);
    SmoothGrid->Iterate();
}

static void Cleanup(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SSmoothGrid *>(UserData);
    SmoothGrid->~SmoothGrid();
}

int main()
{
    alignas(SSmoothGrid) std::array<std::byte, sizeof(SSmoothGrid)> SmoothGrid;
    Rr_AppConfig Config = {};
    Config.Title = "SmoothGrid";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.smoothgrid";
    Config.InitFunc = Init;
    Config.EventFunc = Event;
    Config.IterateFunc = Iterate;
    Config.CleanupFunc = Cleanup;
    Config.UserData = SmoothGrid.data();
    Rr_Run(&Config);
}
