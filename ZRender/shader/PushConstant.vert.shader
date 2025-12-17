#version 460
#pragma shader_stage(vertex)

// push constant 块由 CPU 侧直接塞进命令缓冲区。
// 这里放三组实例位置偏移，分别对应三个实例。
layout(push_constant) uniform pushConstants
{
    vec2 u_Positions[3];
};

// location 0 对应逐顶点输入里的 position。
layout(location = 0) in vec2 i_Position;

// location 1 对应逐顶点输入里的 color。
layout(location = 1) in vec4 i_Color;

// 把颜色传给片段着色器。
layout(location = 0) out vec4 o_Color;

void main()
{
    // gl_InstanceIndex 会告诉我们当前正在处理第几个实例。
    // 用它去索引 push constant 数组，就能取到当前实例对应的偏移量。
    gl_Position = vec4(i_Position + u_Positions[gl_InstanceIndex], 0.0, 1.0);

    // 颜色仍然原样传给片段着色器。
    o_Color = i_Color;
}
