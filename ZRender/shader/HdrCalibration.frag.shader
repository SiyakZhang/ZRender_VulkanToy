#version 460
#pragma shader_stage(fragment)
#include "EOTF.h"

// 接收全屏顶点着色器传来的 UV。
layout(location = 0) in vec2 i_TexCoord;

// 输出到交换链颜色附件。
layout(location = 0) out vec4 o_Color;

// binding 0 上绑定的是那张“几乎纯白”的校准参考图。
layout(binding = 0) uniform sampler2D u_Texture;

// 这个缩放值表示“当前把 SDR 白映射到 10000nit 的多少比例”。
layout(push_constant) uniform pushConstants
{
    float brightnessScale;
};

void main()
{
    // 先按普通方式采样参考图。
    o_Color = texture(u_Texture, i_TexCoord);

    // 纯白背景保持 1.0 不动，只把文字等较暗区域编码进 HDR10。
    if (o_Color.r < 1.0)
        o_Color.rgb = InverseEotf_PQ(o_Color.rgb * brightnessScale);
}
