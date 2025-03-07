#version 460 core

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) uniform UniformData
{
    uint Count; /* [0] == Count */
};

layout(set = 0, binding = 1) buffer Data
{
    uint Buffer[];
};

void main()
{
    for (uint Batch = 0; Batch < Count; Batch += gl_WorkGroupSize.x)
    {
        uint ThreadID = gl_LocalInvocationID.x + Batch;
        if (Batch > 0 && gl_LocalInvocationID.x == 0)
        {
            Buffer[ThreadID] = Buffer[ThreadID - 1] + ThreadID;
        }
        else
        {
            Buffer[ThreadID] = ThreadID;
        }
        barrier();

        uint Steps = uint(log2(gl_WorkGroupSize.x));
        for (uint Step = 0; Step <= Steps; Step++)
        {
            uint Pow = uint(pow(2, Step));
            if (ThreadID + Pow < Count)
            {
                uint Local = Buffer[ThreadID + Pow] + Buffer[ThreadID];
                barrier();
                Buffer[ThreadID + Pow] = Local;
            }
            barrier();
        }
    }
}
