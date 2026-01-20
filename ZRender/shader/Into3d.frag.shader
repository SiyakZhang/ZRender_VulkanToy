#version 460
#pragma shader_stage(fragment)

// 顶点着色器插值得到的面颜色。
layout(location = 0) in vec4 i_Color;

// 写到颜色附件上的最终颜色。
layout(location = 0) out vec4 o_Color;

void main() {
	// 直接显示传入的颜色，便于观察深度遮挡关系。
	o_Color = i_Color;
}
