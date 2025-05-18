#pragma once

// 标准库头文件：后续章节会逐步用到这些基础设施。
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// GLM 默认使用 Vulkan 常见的深度范围 [0, 1]。
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// 在 Windows 上启用 Win32 surface 所需的 Vulkan 平台宏。
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#pragma comment(lib, "vulkan-1.lib")
#endif

// Vulkan 主头文件。
#include <vulkan/vulkan.h>
