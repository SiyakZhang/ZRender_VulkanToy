#version 460
#pragma shader_stage(vertex)

// 把每个顶点对应的颜色传给后续阶段。
layout(location = 0) out vec3 v_Color;

// 这三个位置直接写在着色器里，因此当前示例还不需要顶点缓冲区。
const vec2 positions[3] = vec2[3](
    // 第 0 个顶点放在下方中间。
    vec2( 0.0, -0.5),
    // 第 1 个顶点放在左上角。
    vec2(-0.5,  0.5),
    // 第 2 个顶点放在右上角。
    vec2( 0.5,  0.5)
);

// 给三个顶点各自指定一个基础颜色。
const vec3 colors[3] = vec3[3](
    // 红色偏暖，方便和另外两个顶点区分。
    vec3(1.0, 0.2, 0.2),
    // 绿色顶点。
    vec3(0.2, 1.0, 0.2),
    // 蓝色顶点。
    vec3(0.2, 0.4, 1.0)
);

void main()
{
    // gl_VertexIndex 表示当前这次顶点着色器调用正在处理第几个顶点。
    const int vertexIndex = gl_VertexIndex;

    // 自定义输出变量会继续传给后续图形管线阶段。
    v_Color = colors[vertexIndex];

    // gl_Position 是顶点着色器最重要的内置输出。
    // x 和 y 处于 [-1, 1] 时，顶点会落在 NDC 可见范围内。
    // z 这里固定为 0，表示顶点位于近平面和远平面之间的中间。
    // w 固定为 1，表示这里不额外引入透视除法缩放。
    gl_Position = vec4(positions[vertexIndex], 0.0, 1.0);
}
