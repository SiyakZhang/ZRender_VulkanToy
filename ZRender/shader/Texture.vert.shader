#version 460
#pragma shader_stage(vertex)

// location 0 对应顶点位置。
layout(location = 0) in vec2 i_Position;

// location 1 对应纹理坐标。
layout(location = 1) in vec2 i_TexCoord;

// 把纹理坐标传给片段着色器。
layout(location = 0) out vec2 o_TexCoord;

void main()
{
    // 直接把二维位置补成齐次坐标。
    gl_Position = vec4(i_Position, 0.0, 1.0);

    // 原样把纹理坐标传下去。
    o_TexCoord = i_TexCoord;
}
