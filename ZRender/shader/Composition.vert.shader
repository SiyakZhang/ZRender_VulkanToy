#version 460
#pragma shader_stage(vertex)

// 用 triangle strip 画全屏矩形时，只需要这 4 个角点。
vec2 positions[4] = {
	{-1, -1},
	{-1,  1},
	{ 1, -1},
	{ 1,  1}
};

// 把 NDC 平面坐标传给片段着色器，后面可借它和投影矩阵反推位置。
layout(location = 0) out vec2 o_Position;

void main() {
	// 顶点输出的就是全屏矩形四个角。
	o_Position = positions[gl_VertexIndex];
	gl_Position = vec4(o_Position, 0, 1);
}
