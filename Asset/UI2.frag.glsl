#version 460

// layout(constant_id = 0) const uint CONVERT_TO_SRGB = 0;

layout(set = 0, binding = 1) readonly buffer BClipRects
{
    vec4 ClipRects[];
};

layout(set = 1, binding = 0) uniform sampler2D Atlas;

layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec4 InColor;
layout(location = 3) in flat uint InClipIndex;

layout(location = 0) out vec4 OutColor;

void main() {
    vec4 ClipRect = ClipRects[InClipIndex];
    ClipRect.xy = floor(ClipRect.xy);
    ClipRect.zw = ceil(ClipRect.zw);
    if (InPosition.x < ClipRect.x ||
        InPosition.x > ClipRect.x + ClipRect.z ||
        InPosition.y < ClipRect.y ||
        InPosition.y > ClipRect.y + ClipRect.w)
    {
        discard;
    }
    vec4 Color = InColor;
    vec4 Texture = texture(Atlas, InUV);
    OutColor = Color * Texture;
}
