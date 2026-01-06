#version 460
#pragma shader_stage(fragment)

// 接收顶点着色器传来的纹理坐标。
layout(location = 0) in vec2 i_TexCoord;

// 输出到 0 号颜色附件。
layout(location = 0) out vec4 o_Color;

// binding 0 上绑定的是“带采样器的图像”描述符。
layout(binding = 0) uniform sampler2D u_Texture;

void main()
{
    // 用纹理坐标对贴图采样，并把结果直接写出去。
    o_Color = texture(u_Texture, i_TexCoord);
}
