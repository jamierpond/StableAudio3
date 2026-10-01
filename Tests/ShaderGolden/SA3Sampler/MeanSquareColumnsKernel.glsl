#version 450

layout(std140, set = 0, binding = 16) uniform Uniforms
{
    uint u0;
    uint u1;
    uint u2;
    uint count;
} uniforms;

layout(std430, set = 0, binding = 0) readonly buffer Buffer0
{
    float buffer0[];
};
layout(std430, set = 0, binding = 1) buffer Buffer1
{
    float buffer1[];
};

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    if (gid >= uniforms.count)
        return;
    float v0 = 0.0;
    uint v1 = 0u;
    while ((v1 < uniforms.u0))
    {
        float t0 = buffer0[((v1 * uniforms.u1) + gid)];
        v0 = (v0 + (t0 * t0));
        v1 = (v1 + 1u);
    }
    buffer1[(uniforms.u2 + gid)] = (v0 / float(uniforms.u0));
}
