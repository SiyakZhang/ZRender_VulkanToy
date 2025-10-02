#version 460
#pragma shader_stage(geometry)

// 声明几何着色器接收的输入图元类型是三角形。
layout(triangles) in;

// 声明输出图元类型仍然是三角形带。
// 本例会输出两个三角形，因此最多提交 6 个顶点。
layout(triangle_strip, max_vertices = 6) out;

// 接收顶点着色器传来的颜色。
// 因为当前输入图元是三角形，所以这里要写成数组形式。
layout(location = 0) in vec3 v_Color[];

// 把颜色继续传给片段着色器。
// 名字不需要和片段着色器输入一致，只要 location 和类型一致即可。
layout(location = 0) out vec3 g_Color;

// 计算当前输入三角形三个顶点的重心。
vec4 ComputeCenter()
{
    // 三个顶点位置求平均，就能得到一个位于三角形内部的中心点。
    return (gl_in[0].gl_Position + gl_in[1].gl_Position + gl_in[2].gl_Position) / 3.0;
}

// 把“设置输出变量 + EmitVertex”封装一下，主流程会更容易读。
void EmitTriangleVertex(vec4 position, vec3 color)
{
    // 几何着色器每输出一个顶点，都要先把当前顶点的所有输出重新写好。
    gl_Position = position;
    // 自定义颜色输出同理。
    g_Color = color;
    // 提交当前顶点到正在构建的输出图元。
    EmitVertex();
}

void main()
{
    // gl_in 保存的是前一阶段输出给当前输入图元的所有顶点数据。
    // 对于 triangles 输入来说，下标 0、1、2 就是这个三角形的三个顶点。
    for (int i = 0; i < 3; ++i)
    {
        // 先原样输出一遍外层彩色三角形。
        EmitTriangleVertex(gl_in[i].gl_Position, v_Color[i]);
    }

    // 结束第一个输出图元。
    EndPrimitive();

    // 计算重心，后面把每个顶点往重心方向收缩，生成一个更小的内层三角形。
    const vec4 center = ComputeCenter();

    for (int i = 0; i < 3; ++i)
    {
        // mix(a, b, 0.5) 表示取 a 和 b 的中点。
        const vec4 innerPosition = mix(center, gl_in[i].gl_Position, 0.5);

        // 内层三角形调成更偏白的颜色，方便看出它是几何着色器额外生成的。
        const vec3 innerColor = mix(vec3(1.0), v_Color[i], 0.35);

        // 输出内层三角形的顶点。
        EmitTriangleVertex(innerPosition, innerColor);
    }

    // 结束第二个输出图元。
    EndPrimitive();
}
