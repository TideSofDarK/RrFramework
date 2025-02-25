#version 450
#extension GL_ARB_shading_language_include : require

#include "GS.glsl"

layout(location = 0) out vec4 OutColor;
layout(location = 1) out vec3 OutConic;
layout(location = 2) out vec2 OutCoords;
layout(location = 3) out vec2 OutUV;

const vec2 TriPositions[4] = vec2[4](vec2(-1.0, 1.0), vec2(1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, -1.0));
const uint TriIndices[6] = uint[6](0, 1, 2, 0, 2, 3);

/* https://github.com/limacv/GaussianSplattingViewer/blob/main/shaders/gau_vert.glsl */

mat3 ComputeCov3D(vec3 Scale, vec4 Quat)
{
    mat3 S = mat3(0.f);
    S[0][0] = Scale.x;
    S[1][1] = Scale.y;
    S[2][2] = Scale.z;
    float R = Quat.x;
    float X = Quat.y;
    float Y = Quat.z;
    float Z = Quat.w;

    mat3 MatR = mat3(
            1.f - 2.f * (Y * Y + Z * Z), 2.f * (X * Y - R * Z), 2.f * (X * Z + R * Y),
            2.f * (X * Y + R * Z), 1.f - 2.f * (X * X + Z * Z), 2.f * (Y * Z - R * X),
            2.f * (X * Z - R * Y), 2.f * (Y * Z + R * X), 1.f - 2.f * (X * X + Y * Y)
        );

    mat3 M = S * MatR;
    mat3 Sigma = transpose(M) * M;
    return Sigma;
}

vec3 ComputeCov2D(vec4 MeanView, float FocalX, float FocalY, float TanFOVX, float TanFOVY, mat3 Cov3D)
{
    float LimX = 1.3f * TanFOVX;
    float LimY = 1.3f * TanFOVY;
    float MeanViewXZ = MeanView.x / MeanView.z;
    float MeanViewYZ = MeanView.y / MeanView.z;
    MeanView.x = min(LimX, max(-LimX, MeanViewXZ)) * MeanView.z;
    MeanView.y = min(LimY, max(-LimY, MeanViewYZ)) * MeanView.z;

    mat3 J = mat3(
            FocalX / MeanView.z, 0.0f, -(FocalX * MeanView.x) / (MeanView.z * MeanView.z),
            0.0f, FocalY / MeanView.z, -(FocalY * MeanView.y) / (MeanView.z * MeanView.z),
            0, 0, 0
        );
    mat3 W = transpose(mat3(View));
    mat3 T = W * J;

    mat3 Cov = transpose(T) * transpose(Cov3D) * T;
    Cov[0][0] += 0.3f;
    Cov[1][1] += 0.3f;
    return vec3(Cov[0][0], Cov[0][1], Cov[1][1]);
}

void main()
{
    uint Index = TriIndices[gl_VertexIndex];
    vec2 TriPosition = TriPositions[Index];

    uint InstanceIndex = Indices[gl_InstanceIndex];

    SGPUSplat Splat = Splats[InstanceIndex];

    vec4 Position = vec4(Splat.Position.xyz, 1.f);
    vec4 PositionView = View * Position;
    vec4 PositionScreen = Projection * PositionView;
    PositionScreen.xyz = PositionScreen.xyz / PositionScreen.w;
    PositionScreen.w = 1.f;
    if (any(greaterThan(abs(PositionScreen.xyz), vec3(1.3))))
    {
        gl_Position = vec4(-100, -100, -100, 1);
        return;
    }

    mat3 Cov3D = ComputeCov3D(Splat.Scale.xyz, Splat.Quat);
    vec2 WidthHeight = 2 * HFOVFocal.xy * HFOVFocal.z;
    vec3 Cov2D = ComputeCov2D(PositionView,
            HFOVFocal.z,
            HFOVFocal.z,
            HFOVFocal.x,
            HFOVFocal.y,
            Cov3D);

    float Det = (Cov2D.x * Cov2D.z - Cov2D.y * Cov2D.y);
    if (Det == 0.0f)
        gl_Position = vec4(0.f, 0.f, 0.f, 0.f);

    float DetInv = 1.f / Det;
    OutConic = vec3(Cov2D.z * DetInv, -Cov2D.y * DetInv, Cov2D.x * DetInv);

    vec2 QuadWidthHeightScreen = vec2(3.f * sqrt(Cov2D.x), 3.f * sqrt(Cov2D.z));
    vec2 QuadWidthHeightNDC = QuadWidthHeightScreen / WidthHeight * 2;
    PositionScreen.xy = PositionScreen.xy + TriPosition * QuadWidthHeightNDC;

    OutUV = TriPosition;
    OutCoords = TriPosition * QuadWidthHeightScreen;
    gl_Position = PositionScreen;
    OutColor = Splat.Color;
}
