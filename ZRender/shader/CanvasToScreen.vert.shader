#version 460
#pragma shader_stage(vertex)

// 用 triangle strip 画一个矩形时，只需要这 4 个角点。
vec2 positions[4] = {
	{ 0, 0 },
	{ 0, 1 },
	{ 1, 0 },
	{ 1, 1 }
};

// 传给片段着色器的纹理坐标，范围同样是 0~1。
layout(location = 0) out vec2 o_TexCoord;

// 这里同时传入窗口尺寸和离屏画布尺寸。
layout(push_constant) uniform pushConstants {
	// 用于把矩形从像素坐标换算到 NDC。
	vec2 viewportSize;

	// 用于决定矩形在屏幕上要占多大区域。
	vec2 canvasSize;
};

void main() {
	// 顶点位置和采样坐标都直接复用同一组 0~1 的坐标。
	o_TexCoord = positions[gl_VertexIndex];

	// 先把矩形缩放到画布大小，再按窗口尺寸换算到 NDC。
	gl_Position = vec4(2 * positions[gl_VertexIndex] * canvasSize / viewportSize - 1, 0, 1);
}
