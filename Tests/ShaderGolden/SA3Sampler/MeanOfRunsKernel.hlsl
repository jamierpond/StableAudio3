struct Uniforms
{
    uint u0;
    uint u1;
    uint count;
};

cbuffer UniformsCB : register(b0)
{
    Uniforms uniforms;
};

StructuredBuffer<float> buffer0 : register(t0);
RWStructuredBuffer<float> buffer1 : register(u1);

[numthreads(64, 1, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    uint gid = threadId.x;
    if (gid >= uniforms.count)
        return;
    float v0 = 0.0;
    uint v1 = 0u;
    while ((v1 < uniforms.u0))
    {
        v0 = (v0 + buffer0[((gid * uniforms.u0) + v1)]);
        v1 = (v1 + 1u);
    }
    buffer1[(uniforms.u1 + gid)] = (v0 / float(uniforms.u0));
}
