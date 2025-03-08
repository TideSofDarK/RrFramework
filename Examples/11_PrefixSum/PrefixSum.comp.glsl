#version 460 core

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) uniform SUniformData
{
    uint Count;
};

layout(set = 0, binding = 1) buffer SInputBuffer
{
    uint InputBuffer[];
};

layout(set = 0, binding = 2) buffer SOutputBuffer
{
    uint OutputBuffer[];
};

layout(set = 0, binding = 3) buffer SWorkgroupBuffer
{
    uint WorkgroupBuffer[];
};

shared uint LocalBuffer[gl_WorkGroupSize.x];

void main()
{
    if (gl_GlobalInvocationID.x >= Count)
    {
        return;
    }

    /* Reset buffer with 0..Count values. */

    InputBuffer[gl_GlobalInvocationID.x] = gl_GlobalInvocationID.x;
    barrier();

    /* Init local data. */

    LocalBuffer[gl_LocalInvocationID.x] = InputBuffer[gl_GlobalInvocationID.x];
    barrier();

    uint Steps = uint(log2(Count));
    uint NewValue = 0;
    for (uint Step = 0; Step <= Steps; Step++)
    {
        uint Pow = uint(pow(2, Step));

        if (gl_LocalInvocationID.x < Pow)
        {
            NewValue = LocalBuffer[gl_LocalInvocationID.x];
        }
        else
        {
            NewValue = LocalBuffer[gl_LocalInvocationID.x] + LocalBuffer[gl_LocalInvocationID.x - Pow];
        }

        barrier();
        LocalBuffer[gl_LocalInvocationID.x] = NewValue;
    }

    OutputBuffer[gl_GlobalInvocationID.x] = NewValue;
    barrier();

    if (gl_LocalInvocationID.x == gl_WorkGroupSize.x - 1)
    {
        WorkgroupBuffer[gl_WorkGroupID.x] = OutputBuffer[gl_GlobalInvocationID.x];
    }
    barrier();

    if (gl_WorkGroupID.x > 0)
    {
        for (int Index = 0; Index < gl_WorkGroupID.x; ++Index)
        {
            OutputBuffer[gl_GlobalInvocationID.x] += WorkgroupBuffer[Index];
        }
    }
}
