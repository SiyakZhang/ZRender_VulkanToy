#version 460
#pragma shader_stage(fragment)

// 这次真正干活的是混色状态，所以片段着色器输出什么都无所谓。
layout(location = 0) out vec4 o_Color;

void main() {
	// 输出全 0，只让固定功能混色阶段去做“RGB *= A”。
	o_Color = vec4(0);
}
