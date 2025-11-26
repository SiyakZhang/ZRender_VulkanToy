#version 460
#pragma shader_stage(vertex)

// location 0 对应 CPU 侧顶点结构里的 position。
layout(location = 0) in vec2 i_Position;

// location 1 对应 CPU 侧顶点结构里的 color。
layout(location = 1) in vec4 i_Color;

// 把颜色传给片段着色器。
layout(location = 0) out vec4 o_Color;

void main()
{
    // 把二维位置补成齐次坐标，z 固定为 0，w 固定为 1。
    gl_Position = vec4(i_Position, 0.0, 1.0);

    // 原样把顶点颜色传下去，之后会由光栅化阶段做插值。
    o_Color = i_Color;
}
