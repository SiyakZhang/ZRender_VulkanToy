#version 460
#pragma shader_stage(vertex)

// 这个 uniform block 会从 binding 0 读取数据。
// 由于没有多套描述符集，所以这里省略 set = 0 也没问题。
layout(binding = 0) uniform trianglePosition
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
    // gl_InstanceIndex 仍然表示当前正在绘制第几个实例。
    // 这里改为从 uniform 缓冲区数组里取当前实例对应的偏移量。
    gl_Position = vec4(i_Position + u_Positions[gl_InstanceIndex], 0.0, 1.0);

    // 颜色仍然原样传给片段着色器。
    o_Color = i_Color;
}
