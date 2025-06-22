#pragma once

// 标准库：这里集中放后续各章节都会反复用到的常用头文件。
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <numbers>
#include <numeric>
#include <span>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
#include <type_traits>

// GLM：Vulkan 默认使用 [0, 1] 深度范围，所以先固定这个宏。
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

// 如果以后你更习惯左手坐标系，可以在这里继续打开 GLM_FORCE_LEFT_HANDED。
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

// Vulkan：在 Windows 上需要先打开 Win32 surface 宏。
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#pragma comment(lib, "vulkan-1.lib")
#endif
#include <vulkan/vulkan.h>

template <typename T>
class arrayRef
{
    // 保存首元素指针，但不拥有这段内存。
    T* const pArray = nullptr;

    // 保存元素数量，供遍历和边界推导使用。
    size_t count = 0;

public:
    // 默认构造一个空引用。
    arrayRef() = default;

    // 把单个对象视作长度为 1 的数组。
    arrayRef(T& data) : pArray(&data), count(1)
    {
    }

    // 直接从静态数组推导长度。
    template <size_t elementCount>
    arrayRef(T (&data)[elementCount]) : pArray(data), count(elementCount)
    {
    }

    // 从裸指针和长度构造一个非拥有引用。
    arrayRef(T* pData, size_t elementCount) : pArray(pData), count(elementCount)
    {
    }

    // 允许从非 const 引用转换成 const 引用版本。
    arrayRef(const arrayRef<std::remove_const_t<T>>& other)
        : pArray(other.Pointer()), count(other.Count())
    {
    }

    // 返回内部原始指针。
    T* Pointer() const
    {
        return pArray;
    }

    // 返回数组元素数量。
    size_t Count() const
    {
        return count;
    }

    // 提供下标访问。
    T& operator[](size_t index) const
    {
        return pArray[index];
    }

    // 提供范围 for 所需的 begin。
    T* begin() const
    {
        return pArray;
    }

    // 提供范围 for 所需的 end。
    T* end() const
    {
        return pArray + count;
    }

    // 引用语义下不允许重新赋值到别的数组。
    arrayRef& operator=(const arrayRef&) = delete;
};
