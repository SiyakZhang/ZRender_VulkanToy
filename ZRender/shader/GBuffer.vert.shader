#version 460
#pragma shader_stage(vertex)

// 模型顶点位置。
layout(location = 0) in vec3 i_Position;

// 模型顶点法线。
layout(location = 1) in vec3 i_Normal;

// xyz 为基础颜色，w 为高光强度。
layout(location = 2) in vec4 i_AlbedoSpecular;

// 每个实例整体平移到场景中的不同位置。
layout(location = 3) in vec3 i_InstancePosition;

// 第 0 张 G-Buffer：xyz 存法线，w 存相机空间 z。
layout(location = 0) out vec4 o_NormalZ;

// 第 1 张 G-Buffer：直接存颜色和高光度。
layout(location = 1) out vec4 o_AlbedoSpecular;

// 顶点阶段只需要投影矩阵和观察矩阵。
layout(binding = 0) uniform descriptorConstants_pv {
	mat4 proj;
	mat4 view;
};

void main() {
	// 先把模型顶点平移到对应实例位置。
	vec3 position = i_Position + i_InstancePosition;

	// 转到裁剪空间，供栅格化阶段继续处理。
	gl_Position = proj * view * vec4(position, 1);

	// 法线先直接写入 G-Buffer，后面片段着色器再统一归一化。
	o_NormalZ.xyz = i_Normal;

	// gl_Position.w 正好等于投影前的相机空间 z，可直接留给合成阶段反推位置。
	o_NormalZ.w = gl_Position.w;

	// 颜色与高光度原样输出到第二张 G-Buffer。
	o_AlbedoSpecular = i_AlbedoSpecular;
}
