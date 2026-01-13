#version 460
#pragma shader_stage(fragment)

// 离屏画线阶段只需要输出一个纯色即可。
layout(location = 0) out vec4 o_Color;

void main() {
	// 把线段画成纯红色，后面采样到屏幕时更容易观察。
	o_Color = vec4(1, 0, 0, 1);
}
