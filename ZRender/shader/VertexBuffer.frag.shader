#version 460
#pragma shader_stage(fragment)

// 接收顶点着色器传来的颜色插值结果。
layout(location = 0) in vec4 i_Color;

// 输出到 0 号颜色附件。
layout(location = 0) out vec4 o_Color;

void main()
{
    // 直接把插值后的颜色写出去。
    o_Color = i_Color;
}
