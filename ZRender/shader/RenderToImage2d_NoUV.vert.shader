#version 460
#pragma shader_stage(vertex)

// 这个全屏矩形只负责把目标图像整个覆盖一遍，不需要传 UV。
vec2 positions[4] = {
	{-1, -1},
	{-1,  1},
	{ 1, -1},
	{ 1,  1}
};

void main() {
	// 直接输出全屏矩形四个角的 NDC 坐标。
	gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
