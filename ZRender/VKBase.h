#pragma once

#include "VKHead.h"

namespace vulkan
{
    // 当前阶段先直接使用 VkResult，后面再按教程逐步封装。
    using result_t = VkResult;

    // 统一的默认窗口大小，后面创建窗口时会直接复用。
    inline constexpr VkExtent2D defaultWindowSize = {1280, 720};

    // 统一的输出流，后续错误信息和调试信息都可以走这里。
    inline auto& outStream = std::cout;

    class graphicsBase
    {
    public:
        // 用单例统一管理 Vulkan 初始化阶段产生的全局对象。
        static graphicsBase& Base()
        {
            return singleton;
        }

        // 读取当前准备使用的 Vulkan API 版本号。
        uint32_t ApiVersion() const
        {
            return apiVersion;
        }

        // 读取已经创建好的 Vulkan 实例。
        VkInstance Instance() const
        {
            return instance;
        }

        // 读取已经绑定的窗口 surface。
        VkSurfaceKHR Surface() const
        {
            return surface;
        }

        // 手动写入 surface，供 GLFW 创建窗口 surface 后保存。
        void Surface(VkSurfaceKHR newSurface)
        {
            surface = newSurface;
        }

        // 读取当前选中的物理设备。
        VkPhysicalDevice PhysicalDevice() const
        {
            return physicalDevice;
        }

        // 读取当前创建好的逻辑设备。
        VkDevice Device() const
        {
            return device;
        }

        // 读取图形队列族索引。
        uint32_t QueueFamilyIndex_Graphics() const
        {
            return queueFamilyIndex_graphics;
        }

        // 读取呈现队列族索引。
        uint32_t QueueFamilyIndex_Presentation() const
        {
            return queueFamilyIndex_presentation;
        }

        // 读取交换链句柄。
        VkSwapchainKHR Swapchain() const
        {
            return swapchain;
        }

        // 读取交换链创建信息。
        const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const
        {
            return swapchainCreateInfo;
        }

        // 读取实例层列表，方便排查初始化参数。
        const std::vector<const char*>& InstanceLayers() const
        {
            return instanceLayers;
        }

        // 读取实例扩展列表。
        const std::vector<const char*>& InstanceExtensions() const
        {
            return instanceExtensions;
        }

        // 读取设备扩展列表。
        const std::vector<const char*>& DeviceExtensions() const
        {
            return deviceExtensions;
        }

        // 向实例层列表中追加一个层名。
        void AddInstanceLayer(const char* layerName)
        {
            AddLayerOrExtension(instanceLayers, layerName);
        }

        // 向实例扩展列表中追加一个扩展名。
        void AddInstanceExtension(const char* extensionName)
        {
            AddLayerOrExtension(instanceExtensions, extensionName);
        }

        // 向设备扩展列表中追加一个扩展名。
        void AddDeviceExtension(const char* extensionName)
        {
            AddLayerOrExtension(deviceExtensions, extensionName);
        }

        // 优先查询系统支持的最新 Vulkan API 版本。
        result_t UseLatestApiVersion()
        {
            if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
            {
                return vkEnumerateInstanceVersion(&apiVersion);
            }

            return VK_SUCCESS;
        }

        // 第 3 课会真正实现实例创建，这一课先保留流程入口。
        result_t CreateInstance(VkInstanceCreateFlags = 0)
        {
            outStream << "[ graphicsBase ] INFO\n";
            outStream << "CreateInstance() will be implemented in Ch1-3.\n";
            return VK_RESULT_MAX_ENUM;
        }

        // 第 3 课会真正实现物理设备枚举。
        result_t GetPhysicalDevices()
        {
            outStream << "[ graphicsBase ] INFO\n";
            outStream << "GetPhysicalDevices() will be implemented in Ch1-3.\n";
            return VK_RESULT_MAX_ENUM;
        }

        // 第 3 课会真正实现物理设备选择。
        result_t DeterminePhysicalDevice(uint32_t, bool, bool)
        {
            outStream << "[ graphicsBase ] INFO\n";
            outStream << "DeterminePhysicalDevice() will be implemented in Ch1-3.\n";
            return VK_RESULT_MAX_ENUM;
        }

        // 第 3 课会真正实现逻辑设备创建。
        result_t CreateDevice(VkDeviceCreateFlags = 0)
        {
            outStream << "[ graphicsBase ] INFO\n";
            outStream << "CreateDevice() will be implemented in Ch1-3.\n";
            return VK_RESULT_MAX_ENUM;
        }

        // 第 4 课会真正实现交换链创建。
        result_t CreateSwapchain(bool = true, VkSwapchainCreateFlagsKHR = 0)
        {
            outStream << "[ graphicsBase ] INFO\n";
            outStream << "CreateSwapchain() will be implemented in Ch1-4.\n";
            return VK_RESULT_MAX_ENUM;
        }

    private:
        // 构造函数保持私有，强制外部只能通过 Base() 访问单例。
        graphicsBase() = default;

        // 同一个名字不要重复加入容器，避免创建实例或设备时传重复参数。
        static void AddLayerOrExtension(std::vector<const char*>& container, const char* name)
        {
            for (const char* item : container)
            {
                if (std::string_view(item) == std::string_view(name))
                {
                    return;
                }
            }

            container.push_back(name);
        }

        // 单例对象本体。
        inline static graphicsBase singleton{};

        // API 版本默认从 Vulkan 1.0 起步。
        uint32_t apiVersion = VK_API_VERSION_1_0;

        // 实例阶段对象。
        VkInstance instance = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        // 设备阶段对象。
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;

        // 队列族索引在未确定前都先设为忽略值。
        uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;

        // 交换链阶段对象。
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkSwapchainCreateInfoKHR swapchainCreateInfo{};

        // 初始化时会逐步填充这些层与扩展列表。
        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;
        std::vector<const char*> deviceExtensions;
    };
}
