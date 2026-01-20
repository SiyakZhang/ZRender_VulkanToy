#version 460
#pragma shader_stage(vertex)

// 来自逐顶点缓冲区的模型空间位置。
layout(location = 0) in vec3 i_Position;

// 来自逐顶点缓冲区的面颜色。
layout(location = 1) in vec4 i_Color;

// 来自逐实例缓冲区的整体平移偏移。
layout(location = 2) in vec3 i_InstancePosition;

// 直接把颜色传给片段着色器。
layout(location = 0) out vec4 o_Color;

// 本课只用一块 push constant 传 4x4 投影矩阵。
layout(push_constant) uniform pushConstants {
	mat4 proj;
};

void main() {
	// 先把模型顶点平移到对应实例的位置，再乘投影矩阵。
	gl_Position = proj * vec4(i_Position + i_InstancePosition, 1);

	// 颜色原样输出到下一个着色器阶段。
	o_Color = i_Color;
}
