#pragma once

#include "VKHead.h"

namespace vulkan
{
    // 记录一个 VkFormat 在“像素/分量层面”最常用的几个基础属性。
    struct formatInfo
    {
        // rawDataType 用来描述底层数据更接近整数、浮点还是混合/其他类型。
        enum rawDataType : uint8_t
        {
            other,
            integer,
            floatingPoint
        };

        // 颜色或深度/模板意义上的分量个数。
        uint8_t componentCount = 0;

        // 单个分量占多少字节；若是 packed / compressed 格式，这里可能为 0。
        uint8_t sizePerComponent = 0;

        // 一个像素整体占多少字节；压缩格式或不规则格式可能返回 0。
        uint8_t sizePerPixel = 0;

        // 底层原始数据类型。
        uint8_t rawDataType = other;
    };

    // Vulkan 1.0 的“确定格式”范围到 ASTC_12x12_SRGB_BLOCK 为止。
    constexpr size_t formatInfoCount_v1_0 = size_t(VK_FORMAT_ASTC_12X12_SRGB_BLOCK) + 1;

    // 普通“每个分量等宽”的格式可以统一通过这个辅助函数构造。
    constexpr formatInfo MakeFormatInfo(
        uint8_t componentCount,
        uint8_t sizePerComponent,
        formatInfo::rawDataType rawDataType)
    {
        return {
            componentCount,
            sizePerComponent,
            static_cast<uint8_t>(componentCount * sizePerComponent),
            static_cast<uint8_t>(rawDataType)};
    }

    // packed / depth-stencil 这类“不适合按分量等宽理解”的格式走这个辅助函数。
    constexpr formatInfo MakePackedFormatInfo(
        uint8_t componentCount,
        uint8_t sizePerPixel,
        formatInfo::rawDataType rawDataType)
    {
        return {
            componentCount,
            0,
            sizePerPixel,
            static_cast<uint8_t>(rawDataType)};
    }

    // 返回当前格式最常用的基础描述。
    // 这里先覆盖教程后续会频繁用到的主流格式；未列出的格式返回“未知/不规则”信息。
    constexpr formatInfo FormatInfo(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_UNDEFINED:
            return {};

        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_USCALED:
        case VK_FORMAT_R8_SSCALED:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_S8_UINT:
            return MakeFormatInfo(1, 1, formatInfo::integer);

        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_USCALED:
        case VK_FORMAT_R8G8_SSCALED:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8_SRGB:
            return MakeFormatInfo(2, 1, formatInfo::integer);

        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8_SRGB:
            return MakeFormatInfo(3, 1, formatInfo::integer);

        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return MakeFormatInfo(4, 1, formatInfo::integer);

        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_USCALED:
        case VK_FORMAT_R16_SSCALED:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_D16_UNORM:
            return MakeFormatInfo(1, 2, formatInfo::integer);

        case VK_FORMAT_R16_SFLOAT:
            return MakeFormatInfo(1, 2, formatInfo::floatingPoint);

        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_USCALED:
        case VK_FORMAT_R16G16_SSCALED:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
            return MakeFormatInfo(2, 2, formatInfo::integer);

        case VK_FORMAT_R16G16_SFLOAT:
            return MakeFormatInfo(2, 2, formatInfo::floatingPoint);

        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
            return MakeFormatInfo(3, 2, formatInfo::integer);

        case VK_FORMAT_R16G16B16_SFLOAT:
            return MakeFormatInfo(3, 2, formatInfo::floatingPoint);

        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_USCALED:
        case VK_FORMAT_R16G16B16A16_SSCALED:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
            return MakeFormatInfo(4, 2, formatInfo::integer);

        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return MakeFormatInfo(4, 2, formatInfo::floatingPoint);

        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
            return MakeFormatInfo(1, 4, formatInfo::integer);

        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT:
            return MakeFormatInfo(1, 4, formatInfo::floatingPoint);

        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
            return MakeFormatInfo(2, 4, formatInfo::integer);

        case VK_FORMAT_R32G32_SFLOAT:
            return MakeFormatInfo(2, 4, formatInfo::floatingPoint);

        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
            return MakeFormatInfo(3, 4, formatInfo::integer);

        case VK_FORMAT_R32G32B32_SFLOAT:
            return MakeFormatInfo(3, 4, formatInfo::floatingPoint);

        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
            return MakeFormatInfo(4, 4, formatInfo::integer);

        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return MakeFormatInfo(4, 4, formatInfo::floatingPoint);

        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            return MakePackedFormatInfo(3, 4, formatInfo::floatingPoint);

        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return {1, 3, 4, formatInfo::integer};

        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return MakePackedFormatInfo(2, format == VK_FORMAT_D16_UNORM_S8_UINT ? 3 : 4, formatInfo::integer);

        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return MakePackedFormatInfo(2, 8, formatInfo::other);

        default:
            // 压缩格式、稀有 packed 格式等暂时返回“未知/不规则”信息。
            return {};
        }
    }

    // 把常见的 32 位浮点格式映射到对应的 16 位浮点格式。
    constexpr VkFormat Corresponding16BitFloatFormat(VkFormat format_32BitFloat)
    {
        switch (format_32BitFloat)
        {
        case VK_FORMAT_R32_SFLOAT:
            return VK_FORMAT_R16_SFLOAT;
        case VK_FORMAT_R32G32_SFLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case VK_FORMAT_R32G32B32_SFLOAT:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        default:
            return format_32BitFloat;
        }
    }
}
