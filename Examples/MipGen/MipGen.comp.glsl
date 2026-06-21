#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 32;

layout(local_size_x_id = 0, local_size_y_id = 0, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) restrict readonly uniform image2D SrcImage;
layout(set = 0, binding = 1, rgba8) restrict writeonly uniform image2D DstImage;

void main()
{
    ivec2 Coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 SrcSize = imageSize(SrcImage);
    ivec2 DstSize = imageSize(DstImage);

    if (Coords.x < DstSize.x && Coords.y < DstSize.y)
    {
        ivec2 Offset = ivec2(0, 1);
        vec4 Color0 = imageLoad(SrcImage, Coords * 2 + Offset.xx);
        vec4 Color1 = imageLoad(SrcImage, Coords * 2 + Offset.xy);
        vec4 Color2 = imageLoad(SrcImage, Coords * 2 + Offset.yx);
        vec4 Color3 = imageLoad(SrcImage, Coords * 2 + Offset.yy);
        vec4 FinalColor = (Color0 + Color1 + Color2 + Color3) * 0.25;
        imageStore(DstImage, Coords, FinalColor);
    }
}
