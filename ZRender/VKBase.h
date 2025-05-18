#pragma once

#include "VKHead.h"

namespace vulkan
{
    // 统一的默认窗口大小，后面创建窗口时会直接复用。
    inline constexpr VkExtent2D defaultWindowSize = {1280, 720};

    // 统一的输出流，后续错误信息和调试信息都可以走这里。
    inline auto& outStream = std::cout;
}
