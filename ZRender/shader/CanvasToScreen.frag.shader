#version 460
#pragma shader_stage(fragment)

// 顶点着色器传来的纹理坐标。
layout(location = 0) in vec2 i_TexCoord;

// 最终写到交换链图像上的颜色。
layout(location = 0) out vec4 o_Color;

// 这里采样的就是离屏画布。
layout(binding = 0) uniform sampler2D u_Texture;

// 片段着色器只关心画布大小，所以把偏移到 8 的那段 push constant 重新声明出来。
layout(push_constant) uniform pushConstants {
	layout(offset = 8)
	vec2 canvasSize;
};

void main() {
	// 红色通道采当前像素。
	o_Color = texture(u_Texture, i_TexCoord);

	// 绿色通道向右上偏一点再次采样，方便看出纹理来自离屏画布。
	o_Color.g = texture(u_Texture, i_TexCoord + 8 / canvasSize).r;

	// 蓝色通道向左下偏一点采样，形成简单的偏色效果。
	o_Color.b = texture(u_Texture, i_TexCoord - 8 / canvasSize).r;
}
