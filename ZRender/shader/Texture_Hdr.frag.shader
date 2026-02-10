#version 460
#pragma shader_stage(fragment)
#include "EOTF.h"

// 接收普通贴图顶点着色器传来的 UV。
layout(location = 0) in vec2 i_TexCoord;

// 输出到交换链颜色附件。
layout(location = 0) out vec4 o_Color;

// binding 0 绑定 HDR 贴图采样器。
layout(binding = 0) uniform sampler2D u_Texture;

// 这个缩放值会把线性 HDR 场景亮度压缩到当前显示器可接受的范围。
layout(push_constant) uniform pushConstants
{
    float brightnessScale;
};

float Reinhard(float v)
{
    // 最简单的色调映射之一：高亮会逐渐压近 1。
    return v / (1.0 + v);
}

vec3 Reinhard(vec3 v)
{
    // RGB 三个通道分别做同样的压缩。
    return vec3(Reinhard(v.r), Reinhard(v.g), Reinhard(v.b));
}

void main()
{
    // 先取出线性 HDR 贴图里的浮点亮度。
    o_Color = texture(u_Texture, i_TexCoord);

    // 先做一次简单的 Reinhard 色调映射，再编码成 HDR10 使用的 PQ 数值。
    o_Color.rgb = InverseEotf_PQ(Reinhard(o_Color.rgb) * brightnessScale);
}
