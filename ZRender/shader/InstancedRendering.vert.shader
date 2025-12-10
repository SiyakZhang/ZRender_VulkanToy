#version 460
#pragma shader_stage(vertex)

// location 0 对应逐顶点输入里的 position。
layout(location = 0) in vec2 i_Position;

// location 1 对应逐顶点输入里的 color。
layout(location = 1) in vec4 i_Color;

// location 2 对应逐实例输入里的位置偏移。
layout(location = 2) in vec2 i_InstanceOffset;

// 把颜色传给片段着色器。
layout(location = 0) out vec4 o_Color;

void main()
{
    // 共用三角形顶点坐标，再加上当前实例自己的偏移量，
    // 就能得到这个实例里当前顶点最终要落到屏幕上的位置。
    gl_Position = vec4(i_Position + i_InstanceOffset, 0.0, 1.0);

    // 颜色仍然原样传给片段着色器。
    o_Color = i_Color;
}
