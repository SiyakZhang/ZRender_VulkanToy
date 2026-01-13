#version 460
#pragma shader_stage(vertex)

// 这一课用 push constant 直接把画布大小和两端点坐标传给顶点着色器。
layout(push_constant) uniform pushConstants {
	// 画布尺寸用于把像素坐标换算到 NDC。
	vec2 viewportSize;

	// 两个端点分别对应一条线段的起点和终点。
	vec2 offsets[2];
};

void main() {
	// gl_VertexIndex 在这条线里只会取 0 和 1。
	// 先把像素坐标除以画布尺寸归一化到 0~1，再映射到 -1~1。
	gl_Position = vec4(2 * offsets[gl_VertexIndex] / viewportSize - 1, 0, 1);
}
