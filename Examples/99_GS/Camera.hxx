#pragma once

#include <Rr/Rr.h>

#include "Input.hxx"

struct SCamera
{
    enum class EType
    {
        FP,
        ORBIT,
        TOPDOWN,
    };

    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{};

    Rr_Vec3 OrbitCenter = { 0.0f, 3.0f, 0.0f };
    float OrbitDistance = 10.0f;
    float OrbitAcceleration = 0.1f;
    Rr_Vec2 OrbitVelocity{};

    float TDHeight = 10.0f;
    float TDPitch = 45.0f * Rr_DegToRad;
    float TDYaw = 45.0f * Rr_DegToRad;
    Rr_Vec3 TDPosition{};

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    float HTanY;
    float HTanX;
    float FocalZ;

    EType Type = EType::TOPDOWN;

    void SetPerspective(float FOVDegrees, Rr_IntVec2 Size, float Near, float Far)
    {
        ProjMatrix = Rr_Perspective_LH_ZO(
            Rr_AngleDeg(FOVDegrees),
            (float)Size.X / (float)Size.Y,
            Near,
            Far);

        HTanY = tanf(Rr_AngleDeg(FOVDegrees) / 2.0);
        HTanX = HTanY / (float)Size.Y * (float)Size.X;
        FocalZ = (float)Size.Y / (2 * HTanY);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void Update(Rr_App *App, Rr_InputState *State)
    {
        Rr_KeyStates Keys = State->Keys;

        if(Rr_GetKeyState(Keys, EIA_CAMERA) == RR_KEYSTATE_PRESSED)
        {
            Type = (EType)(((uint32_t)Type + 1) % 3);
        }

        switch(Type)
        {
            case EType::FP:
            {
                Rr_Vec3 CameraForward = GetForwardVector();
                Rr_Vec3 CameraLeft = GetRightVector();
                constexpr float CameraSpeed = 0.02f;
                if(Rr_GetKeyState(Keys, EIA_UP) == RR_KEYSTATE_HELD)
                {
                    Position -= CameraForward * CameraSpeed;
                }
                if(Rr_GetKeyState(Keys, EIA_LEFT) == RR_KEYSTATE_HELD)
                {
                    Position -= CameraLeft * CameraSpeed;
                }
                if(Rr_GetKeyState(Keys, EIA_DOWN) == RR_KEYSTATE_HELD)
                {
                    Position += CameraForward * CameraSpeed;
                }
                if(Rr_GetKeyState(Keys, EIA_RIGHT) == RR_KEYSTATE_HELD)
                {
                    Position += CameraLeft * CameraSpeed;
                }

                if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
                {
                    Rr_SetRelativeMouseMode(App, true);
                    constexpr float Sensitivity = 0.2f;
                    Yaw = Rr_WrapMax(
                        Yaw - (State->MousePositionDelta.X * Sensitivity),
                        360.0f);
                    Pitch = Rr_WrapMinMax(
                        Pitch - (State->MousePositionDelta.Y * Sensitivity),
                        -90.0f,
                        90.0f);
                }
                else
                {
                    Rr_SetRelativeMouseMode(App, false);
                }

                float CosPitch = cosf(Pitch * Rr_DegToRad);
                float SinPitch = sinf(Pitch * Rr_DegToRad);
                float CosYaw = cosf(Yaw * Rr_DegToRad);
                float SinYaw = sinf(Yaw * Rr_DegToRad);

                Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
                Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
                Rr_Vec3 ZAxis{ SinYaw * CosPitch,
                               -SinPitch,
                               CosPitch * CosYaw };

                ViewMatrix = {
                    XAxis.X,
                    YAxis.X,
                    ZAxis.X,
                    0.0f,
                    XAxis.Y,
                    YAxis.Y,
                    ZAxis.Y,
                    0.0f,
                    XAxis.Z,
                    YAxis.Z,
                    ZAxis.Z,
                    0.0f,
                    -Rr_Dot(XAxis, Position),
                    -Rr_Dot(YAxis, Position),
                    -Rr_Dot(ZAxis, Position),
                    1.0f,
                };
            }
            break;
            case EType::ORBIT:
            {
                if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
                {
                    Rr_SetRelativeMouseMode(App, true);
                    constexpr float Sensitivity = 0.05f;

                    OrbitVelocity.X -=
                        (State->MousePositionDelta.Y * Sensitivity);
                    OrbitVelocity.Y -=
                        (State->MousePositionDelta.X * Sensitivity);
                }
                else
                {
                    Rr_SetRelativeMouseMode(App, false);
                }

                OrbitVelocity.X *= 0.9f;
                OrbitVelocity.Y *= 0.9f;

                Pitch += OrbitVelocity.X;
                Yaw += OrbitVelocity.Y;

                Yaw = Rr_WrapMax(Yaw, 360.0f);
                Pitch = RR_CLAMP(-89.99f, Pitch, 0.0f);

                Rr_Vec3 Eye = { 0.0f, 0.0f, -1.0f };
                Eye = Rr_RotateV3AxisAngle_RH(
                    Eye,
                    { 0.0f, 1.0f, 0.0f },
                    Yaw * Rr_DegToRad);
                Eye = Rr_RotateV3AxisAngle_RH(
                    Eye,
                    Rr_Cross(Eye, { 0.0f, 1.0f, 0.0f }),
                    -Pitch * Rr_DegToRad);

                Eye *= OrbitDistance;
                Eye += OrbitCenter;

                ViewMatrix =
                    Rr_LookAt_RH(Eye, OrbitCenter, { 0.0f, 1.0f, 0.0f });
            }
            break;
            case EType::TOPDOWN:
            {
                if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
                {
                    Rr_SetRelativeMouseMode(App, true);
                    constexpr float Sensitivity = 0.1f;
                    TDPosition.X -= (State->MousePositionDelta.X * Sensitivity);
                    TDPosition.Z -= (State->MousePositionDelta.Y * Sensitivity);
                }
                else
                {
                    Rr_SetRelativeMouseMode(App, false);
                }

                ViewMatrix =
                    Rr_Rotate_LH(-TDPitch, { 1.0f, 0.0f, 0.0f }) *
                    Rr_Translate({ TDPosition.X, -TDHeight, TDPosition.Z }) *
                    Rr_Rotate_LH(TDYaw, { 0.0f, 1.0f, 0.0f });
            }
            break;
            default:
            {
                std::abort();
            }
            break;
        }
    }
};
