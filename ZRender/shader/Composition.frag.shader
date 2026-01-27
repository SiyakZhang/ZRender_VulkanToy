#version 460
#pragma shader_stage(fragment)

struct light {
	vec3 position;
	vec3 color;
	float strength;
};

// 最大灯光数量和高光指数都做成 specialization constant，便于后续按需改。
layout(constant_id = 0) const uint maxLightCount = 32;
layout(constant_id = 1) const uint shininess = 32;

// 从全屏矩形顶点着色器传来的 NDC 平面坐标。
layout(location = 0) in vec2 i_Position;

// 最终输出到交换链图像的颜色。
layout(location = 0) out vec4 o_Color;

// 合成阶段要读取完整场景常量。
layout(binding = 0) uniform descriptorConstants {
	mat4 proj;
	mat4 view;
	int lightCount;
	light lights[maxLightCount];
};

// 绑定 1 是两张输入附件组成的数组。
layout(binding = 1, input_attachment_index = 0) uniform subpassInput u_GBuffers[2];

void main() {
	// 先反求相机在世界空间中的位置，后面算高光会用到。
	mat4 inverseView = inverse(view);
	vec3 cameraPosition = vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]);

	// 从第一张 G-Buffer 取出相机空间 z，再结合投影矩阵和屏幕位置反推当前位置。
	vec3 position;
	position.z = subpassLoad(u_GBuffers[0]).w;
	position.x = (i_Position.x - proj[2][0]) * position.z / proj[0][0];
	position.y = (i_Position.y - proj[2][1]) * position.z / proj[1][1];
	position = (inverseView * vec4(position, 1)).xyz;

	// 第一张 G-Buffer 的 xyz 是法线，第二张 G-Buffer 的 xyz/w 是颜色和高光度。
	vec3 normal = normalize(subpassLoad(u_GBuffers[0]).xyz);
	vec3 albedo = subpassLoad(u_GBuffers[1]).xyz;
	float specular = subpassLoad(u_GBuffers[1]).w;

	// 先把最终颜色清成黑色，再逐灯累加贡献。
	o_Color = vec4(0, 0, 0, 1);

	for (uint i = 0; i < lightCount; i++) {
		// 先算从当前片段指向光源的向量和基于距离平方反比的光强。
		vec3 toLight = lights[i].position - position;
		vec3 lightIntensity = lights[i].color * lights[i].strength / pow(length(toLight), 2);
		toLight = normalize(toLight);

		// 用 Blinn-Phong 的 halfway 向量近似高光方向。
		vec3 toCamera = normalize(cameraPosition - position);
		vec3 halfway = normalize(toCamera + toLight);

		// 漫反射和高光项分别累加到输出颜色。
		o_Color.rgb += lightIntensity * (
			albedo * max(dot(normal, toLight), 0) +
			specular * pow(max(dot(normal, halfway), 0), shininess));
	}
}
