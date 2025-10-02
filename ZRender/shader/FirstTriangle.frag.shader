#version 460
#pragma shader_stage(fragment)

// 接收上一个着色器阶段在 location = 0 输出的颜色。
// 当前这一版上一个阶段是几何着色器；如果拿掉几何阶段，也可以直接来自顶点着色器。
// 栅格化阶段会在三角形内部自动对这个值做插值。
layout(location = 0) in vec3 v_Color;

// 输出到第 0 个颜色附件，也就是当前交换链图像。
layout(location = 0) out vec4 o_Color;

void main()
{
    // 把插值后的 RGB 颜色补上 alpha 后输出。
    // alpha 设为 1 表示这个片段完全不透明。
    o_Color = vec4(v_Color, 1.0);
}
