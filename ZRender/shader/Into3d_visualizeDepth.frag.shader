#version 460
#pragma shader_stage(fragment)

// 深度可视化版不需要输入颜色，但保留 location 0 也不影响使用。
layout(location = 0) in vec4 i_Color;

// 仍然输出到 0 号颜色附件。
layout(location = 0) out vec4 o_Color;

void main() {
	// gl_FragCoord.z 就是当前片段写入深度缓冲前要比较的深度值。
	// 这里把它复制到 RGB 三个通道，直接显示成灰度图。
	o_Color = vec4(gl_FragCoord.z.xxx, 1);
}
