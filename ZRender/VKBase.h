#pragma once
#pragma once
#include "VKHead.h"

namespace vulkan
{
    class graphicsBase
    {
        //静态变量
        static graphicsBase singleton;
        //--------------------
        graphicsBase() = default;
        graphicsBase(graphicsBase&&) = delete;

        ~graphicsBase()
        {
            /*待Ch1-4填充*/
        }

    public:
        //静态函数
        //该函数用于访问单例
        static graphicsBase& Base()
        {
            return singleton;
        }

    private:
        //单例类对象是静态的，未设定初始值亦无构造函数的成员会被零初始化
        VkInstance instance;
        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

        //该函数用于向instanceLayers或instanceExtensions容器中添加字符串指针，并确保不重复
        static void AddLayerOrExtension(std::vector<const char*>& container, const char* name)
        {
            for (auto& i : container)
                if (!strcmp(name, i)) //strcmp(...)在字符串匹配时返回0
                    return; //如果层/扩展的名称已在容器中，直接返回
            container.push_back(name);
        }

    public:
        //Getter
        VkInstance Instance() const
        {
            return instance;
        }

        const std::vector<const char*>& InstanceLayers() const
        {
            return instanceLayers;
        }

        const std::vector<const char*>& InstanceExtensions() const
        {
            return instanceExtensions;
        }

        //以下函数用于创建Vulkan实例前
        void AddInstanceLayer(const char* layerName)
        {
            AddLayerOrExtension(instanceLayers, layerName);
        }

        void AddInstanceExtension(const char* extensionName)
        {
            AddLayerOrExtension(instanceExtensions, extensionName);
        }

        //该函数用于创建Vulkan实例
        VkResult CreateInstance(VkInstanceCreateFlags flags = 0)
        {
#ifndef NDEBUG
            AddInstanceLayer("VK_LAYER_KHRONOS_validation");
            AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
            VkApplicationInfo applicationInfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = apiVersion
            };
            const VkInstanceCreateInfo instanceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .flags = flags,
                .pApplicationInfo = &applicationInfo,
                .enabledLayerCount = uint32_t(instanceLayers.size()),
                .ppEnabledLayerNames = instanceLayers.data(),
                .enabledExtensionCount = uint32_t(instanceExtensions.size()),
                .ppEnabledExtensionNames = instanceExtensions.data()
            };
            if (const VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance); result != VK_SUCCESS)
            {
                std::cout << std::format(
                    "[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
                return result;
            }
            //成功创建Vulkan实例后，输出Vulkan版本
            std::cout << std::format(
                "Vulkan API Version: {}.{}.{}\n",
                VK_VERSION_MAJOR(apiVersion),
                VK_VERSION_MINOR(apiVersion),
                VK_VERSION_PATCH(apiVersion));
#ifndef NDEBUG
            //创建完Vulkan实例后紧接着创建debug messenger
            CreateDebugMessenger();
#endif
            return VK_SUCCESS;
        }

        //以下函数用于创建Vulkan实例失败后
        VkResult CheckInstanceLayers(std::span<const char*> layersToCheck)
        {
            /*待Ch1-3填充*/
        }

        void InstanceLayers(const std::vector<const char*>& layerNames)
        {
            instanceLayers = layerNames;
        }

        VkResult CheckInstanceExtensions(std::span<const char*> extensionsToCheck,
                                         const char* layerName = nullptr) const
        {
            /*待Ch1-3填充*/
        }

        void InstanceExtensions(const std::vector<const char*>& extensionNames)
        {
            instanceExtensions = extensionNames;
        }

    private:
        VkDebugUtilsMessengerEXT debugMessenger;
        //以下函数用于创建debug messenger
        VkResult CreateDebugMessenger()
        {
            /*待Ch1-3填充*/
        }

    private:
        VkSurfaceKHR surface;

    public:
        //Getter
        VkSurfaceKHR Surface() const
        {
            return surface;
        }

        //该函数用于选择物理设备前
        void Surface(VkSurfaceKHR surface)
        {
            if (!this->surface)
                this->surface = surface;
        }

        VkResult UseLatestApiVersion()
        {
            if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
                return vkEnumerateInstanceVersion(&apiVersion);
            return VK_SUCCESS;
        }

    private:
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
        std::vector<VkPhysicalDevice> availablePhysicalDevices;

        VkDevice device;
        //有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED（为UINT32_MAX）为队列族索引的默认值
        uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
        VkQueue queue_graphics;
        VkQueue queue_presentation;
        VkQueue queue_compute;

        std::vector<const char*> deviceExtensions;

        //该函数被DeterminePhysicalDevice(...)调用，用于检查物理设备是否满足所需的队列族类型，并将对应的队列族索引返回到queueFamilyIndices，执行成功时直接将索引写入相应成员变量
        VkResult GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue,
                                       bool enableComputeQueue, uint32_t (&queueFamilyIndices)[3])
        {
            /*待Ch1-3填充*/
        }

    public:
        //Getter
        VkPhysicalDevice PhysicalDevice() const
        {
            return physicalDevice;
        }

        const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const
        {
            return physicalDeviceProperties;
        }

        const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const
        {
            return physicalDeviceMemoryProperties;
        }

        VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const
        {
            return availablePhysicalDevices[index];
        }

        uint32_t AvailablePhysicalDeviceCount() const
        {
            return uint32_t(availablePhysicalDevices.size());
        }

        VkDevice Device() const
        {
            return device;
        }

        uint32_t QueueFamilyIndex_Graphics() const
        {
            return queueFamilyIndex_graphics;
        }

        uint32_t QueueFamilyIndex_Presentation() const
        {
            return queueFamilyIndex_presentation;
        }

        uint32_t QueueFamilyIndex_Compute() const
        {
            return queueFamilyIndex_compute;
        }

        VkQueue Queue_Graphics() const
        {
            return queue_graphics;
        }

        VkQueue Queue_Presentation() const
        {
            return queue_presentation;
        }

        VkQueue Queue_Compute() const
        {
            return queue_compute;
        }

        const std::vector<const char*>& DeviceExtensions() const
        {
            return deviceExtensions;
        }

        //该函数用于创建逻辑设备前
        void AddDeviceExtension(const char* extensionName)
        {
            AddLayerOrExtension(deviceExtensions, extensionName);
        }

        //该函数用于获取物理设备
        VkResult GetPhysicalDevices()
        {
            /*待Ch1-3填充*/
        }

        //该函数用于指定所用物理设备并调用GetQueueFamilyIndices(...)取得队列族索引
        VkResult DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue,
                                         bool enableComputeQueue = true)
        {
            /*待Ch1-3填充*/
        }

        //该函数用于创建逻辑设备，并取得队列
        VkResult CreateDevice(VkDeviceCreateFlags flags = 0)
        {
            /*待Ch1-3填充*/
        }

        //以下函数用于创建逻辑设备失败后
        VkResult CheckDeviceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const
        {
            /*待Ch1-3填充*/
        }

        void DeviceExtensions(const std::vector<const char*>& extensionNames)
        {
            deviceExtensions = extensionNames;
        }

    private:
        std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;

        VkSwapchainKHR swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        //保存交换链的创建信息以便重建交换链
        VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

        //该函数被CreateSwapchain(...)和RecreateSwapchain()调用
        VkResult CreateSwapchain_Internal()
        {
            /*待Ch1-4填充*/
        }

    public:
        //Getter
        const VkFormat& AvailableSurfaceFormat(uint32_t index) const
        {
            return availableSurfaceFormats[index].format;
        }

        const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const
        {
            return availableSurfaceFormats[index].colorSpace;
        }

        uint32_t AvailableSurfaceFormatCount() const
        {
            return uint32_t(availableSurfaceFormats.size());
        }

        VkSwapchainKHR Swapchain() const
        {
            return swapchain;
        }

        VkImage SwapchainImage(uint32_t index) const
        {
            return swapchainImages[index];
        }

        VkImageView SwapchainImageView(uint32_t index) const
        {
            return swapchainImageViews[index];
        }

        uint32_t SwapchainImageCount() const
        {
            return uint32_t(swapchainImages.size());
        }

        const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const
        {
            return swapchainCreateInfo;
        }

        VkResult GetSurfaceFormats()
        {
            /*待Ch1-4填充*/
        }

        VkResult SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
        {
            /*待Ch1-4填充*/
        }

        //该函数用于创建交换链
        VkResult CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0)
        {
            /*待Ch1-4填充*/
        }

        //该函数用于重建交换链
        VkResult RecreateSwapchain()
        {
            /*待Ch1-4填充*/
        }

    private:
        uint32_t apiVersion = VK_API_VERSION_1_0;

    public:
        //Getter
        uint32_t ApiVersion() const
        {
            return apiVersion;
        }

        VkResult UseLatestApiVersion()
        {
            /*待Ch1-3填充*/
        }
    };

    inline graphicsBase graphicsBase::singleton;
}
