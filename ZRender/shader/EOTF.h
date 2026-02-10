// sRGB 电光传递函数：
// 把 sRGB 编码数值还原成线性亮度。
float Eotf_sRGB(float v)
{
    if (v <= 0.04045f)
        return v / 12.92f;

    return pow((v + 0.055f) / 1.055f, 2.4f);
}

// vec3 版本只是把三个通道分别做同样的换算。
vec3 Eotf_sRGB(vec3 v)
{
    return vec3(Eotf_sRGB(v.x), Eotf_sRGB(v.y), Eotf_sRGB(v.z));
}

// sRGB 逆 EOTF：
// 把线性亮度编码回 sRGB 数值。
float InverseEotf_sRGB(float v)
{
    if (v <= 0.0031308f)
        return v * 12.92f;

    return 1.055f * pow(v, 1.0f / 2.4f) - 0.055f;
}

// vec3 版本同理，逐通道处理。
vec3 InverseEotf_sRGB(vec3 v)
{
    return vec3(InverseEotf_sRGB(v.x), InverseEotf_sRGB(v.y), InverseEotf_sRGB(v.z));
}

// PQ EOTF：
// 把 HDR10 的 PQ 编码值还原成线性亮度。
float Eotf_PQ(float v)
{
    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 4096.0f * 128.0f;
    const float c1 = 3424.0f / 4096.0f;
    const float c2 = 2413.0f / 4096.0f * 32.0f;
    const float c3 = 2392.0f / 4096.0f * 32.0f;

    v = pow(v, 1.0f / m2);

    if (v <= c1)
        return 0.0f;

    return pow((v - c1) / (c2 - c3 * v), 1.0f / m1);
}

// vec3 版本的 PQ EOTF。
vec3 Eotf_PQ(vec3 v)
{
    return vec3(Eotf_PQ(v.x), Eotf_PQ(v.y), Eotf_PQ(v.z));
}

// PQ 逆 EOTF：
// 把线性亮度重新编码成 HDR10 要求的 PQ 数值。
float InverseEotf_PQ(float v)
{
    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 4096.0f * 128.0f;
    const float c1 = 3424.0f / 4096.0f;
    const float c2 = 2413.0f / 4096.0f * 32.0f;
    const float c3 = 2392.0f / 4096.0f * 32.0f;

    v = pow(v, m1);
    return pow((c1 + c2 * v) / (1.0f + c3 * v), m2);
}

// vec3 版本的 PQ 逆 EOTF。
vec3 InverseEotf_PQ(vec3 v)
{
    return vec3(InverseEotf_PQ(v.x), InverseEotf_PQ(v.y), InverseEotf_PQ(v.z));
}
