#version 460
#pragma shader_stage(vertex)

// 这 4 个点正好组成一个覆盖全屏的 triangle strip。
vec2 positions[4] = {
    {-1, -1},
    {-1,  1},
    { 1, -1},
    { 1,  1}
};

// 把由顶点位置推出来的 UV 传给片段着色器。
layout(location = 0) out vec2 o_UV;

void main()
{
    // gl_VertexIndex 依次取 0~3，对应全屏矩形四个角。
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

    // NDC 的 [-1, 1] 区间线性映射到纹理坐标的 [0, 1] 区间。
    o_UV = positions[gl_VertexIndex] * 0.5 + 0.5;
}
