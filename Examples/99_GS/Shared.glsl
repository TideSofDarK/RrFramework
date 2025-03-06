struct SGPUSplat
{
    vec4 Position;
    vec4 Quat;
    vec4 Scale;
    vec4 Color;
};

struct SSortEntry
{
    uint Index;
    float Z;
};

bool IsPointInFrustum(vec3 Point, mat4 ViewProjection) {
    vec4 LeftPlane = ViewProjection[3] + ViewProjection[0];
    vec4 RightPlane = ViewProjection[3] - ViewProjection[0];
    vec4 BottomPlane = ViewProjection[3] + ViewProjection[1];
    vec4 TopPlane = ViewProjection[3] - ViewProjection[1];
    vec4 NearPlane = ViewProjection[2] + ViewProjection[3];
    vec4 FarPlane = ViewProjection[3] - ViewProjection[2];

    if (dot(LeftPlane, vec4(Point, 1.0)) < 0.0) return false;
    if (dot(RightPlane, vec4(Point, 1.0)) < 0.0) return false;
    if (dot(BottomPlane, vec4(Point, 1.0)) < 0.0) return false;
    if (dot(TopPlane, vec4(Point, 1.0)) < 0.0) return false;
    if (dot(NearPlane, vec4(Point, 1.0)) < 0.0) return false;
    if (dot(FarPlane, vec4(Point, 1.0)) < 0.0) return false;

    return true;
}
