#version 460
#pragma shader_stage(fragment)

// 顶点阶段整理好的法线和相机空间 z。
layout(location = 0) in vec4 i_NormalZ;

// 顶点阶段传下来的颜色和高光度。
layout(location = 1) in vec4 i_AlbedoSpecular;

// 直接写入第 0 张 G-Buffer。
layout(location = 0) out vec4 o_NormalZ;

// 直接写入第 1 张 G-Buffer。
layout(location = 1) out vec4 o_AlbedoSpecular;

void main() {
	// 这一阶段不做光照，只负责把几何信息保存下来。
	o_NormalZ = i_NormalZ;
	o_AlbedoSpecular = i_AlbedoSpecular;
}
