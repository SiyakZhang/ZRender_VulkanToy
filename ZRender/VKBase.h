#pragma once
#include "VKHead.h"
#define VK_RESULT_THROW

#define DestroyHandleBy(Func) if (handle) { Func(graphicsBase::Base().Device(), handle, nullptr); handle = VK_NULL_HANDLE; }
#define MoveHandle handle = other.handle; other.handle = VK_NULL_HANDLE;
#define DefineHandleTypeOperator operator decltype(handle)() const { return handle; }
#define DefineAddressFunction const decltype(handle)* Address() const { return &handle; }
#define ExecuteOnce(...) { static bool executed = false; if (executed) return __VA_ARGS__; executed = true; }

#ifndef NDEBUG
#define ENABLE_DEBUG_MESSENGER true
#else
#define ENABLE_DEBUG_MESSENGER false
#endif

namespace vulkan
{
    constexpr VkExtent2D defaultWindowSize = {1280, 720};
    inline auto& outStream = std::cout;

#ifdef VK_RESULT_THROW  //情况1：根据函数返回值确定是否抛异常
    class result_t
    {
        VkResult result;

    public:
        static void (*callback_throw)(VkResult);

        result_t(VkResult result) : result(result)
        {
        }

        result_t(result_t&& other) noexcept : result(other.result)
        {
            other.result = VK_SUCCESS;
        }

        ~result_t() noexcept(false)
        {
            if (uint32_t(result) < VK_RESULT_MAX_ENUM)
                return;
            if (callback_throw)
                callback_throw(result);
            throw result;
        }

        operator VkResult()
        {
            VkResult result = this->result;
            this->result = VK_SUCCESS;
            return result;
        }
    };

    inline void (*result_t::callback_throw)(VkResult);

    //情况2：若抛弃函数返回值，让编译器发出警告
#elif defined VK_RESULT_NODISCARD
    struct [[nodiscard]] result_t {
        VkResult result;
        result_t(VkResult result) :result(result) {}
        operator VkResult() const { return result; }
    };
#pragma warning(disable:4834)
#pragma warning(disable:6031)

//情况3：啥都不干
#else
    using result_t = VkResult;
#endif

    class graphicsBase
    {
        //静态变量
        static graphicsBase singleton;
        //--------------------
        graphicsBase() = default;
        graphicsBase(graphicsBase&&) = delete;

        ~graphicsBase()
        {
            if (!instance)
                return;
            if (device)
            {
                WaitIdle();
                if (swapchain)
                {
                    for (auto& i : callbacks_destroySwapchain)
                        i();
                    for (auto& i : swapchainImageViews)
                        if (i)
                            vkDestroyImageView(device, i, nullptr);
                    vkDestroySwapchainKHR(device, swapchain, nullptr);
                }
                for (auto& i : callbacks_destroyDevice)
                    i();
                vkDestroyDevice(device, nullptr);
            }
            if (surface)
                vkDestroySurfaceKHR(instance, surface, nullptr);
            if (debugMessenger)
            {
                PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessenger =
                    reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (DestroyDebugUtilsMessenger)
                    DestroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
            }
            vkDestroyInstance(instance, nullptr);
        }

    public:
        //静态函数
        //该函数用于访问单例
        static graphicsBase& Base()
        {
            return singleton;
        }

        void Terminate()
        {
            this->~graphicsBase();
            instance = VK_NULL_HANDLE;
            physicalDevice = VK_NULL_HANDLE;
            device = VK_NULL_HANDLE;
            surface = VK_NULL_HANDLE;
            swapchain = VK_NULL_HANDLE;
            swapchainImages.resize(0);
            swapchainImageViews.resize(0);
            swapchainCreateInfo = {};
            debugMessenger = VK_NULL_HANDLE;
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
        result_t CreateInstance(VkInstanceCreateFlags flags = 0)
        {
            if constexpr (ENABLE_DEBUG_MESSENGER)
                AddInstanceLayer("VK_LAYER_KHRONOS_validation"),
                    AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            VkApplicationInfo applicatianInfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = apiVersion
            };
            VkInstanceCreateInfo instanceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .flags = flags,
                .pApplicationInfo = &applicatianInfo,
                .enabledLayerCount = uint32_t(instanceLayers.size()),
                .ppEnabledLayerNames = instanceLayers.data(),
                .enabledExtensionCount = uint32_t(instanceExtensions.size()),
                .ppEnabledExtensionNames = instanceExtensions.data()
            };
            if (const VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance); result != VK_SUCCESS)
            {
                outStream << std::format(
                    "[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
                return result;
            }
            //成功创建Vulkan实例后，输出Vulkan版本
            outStream << std::format(
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
        result_t CheckInstanceLayers(std::span<const char*> layersToCheck)
        {
            uint32_t layerCount;
            std::vector<VkLayerProperties> availableLayers;
            if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
                return result;
            }
            if (layerCount)
            {
                availableLayers.resize(layerCount);
                if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()))
                {
                    outStream << std::format(
                        "[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n",
                        int32_t(result));
                    return result;
                }
                for (auto& i : layersToCheck)
                {
                    bool found = false;
                    for (auto& j : availableLayers)
                        if (!strcmp(i, j.layerName))
                        {
                            found = true;
                            break;
                        }
                    if (!found)
                        i = nullptr;
                }
            }
            else
                for (auto& i : layersToCheck)
                    i = nullptr;
            return VK_SUCCESS;
        }

        void InstanceLayers(const std::vector<const char*>& layerNames)
        {
            instanceLayers = layerNames;
        }

        result_t CheckInstanceExtensions(std::span<const char*> extensionsToCheck,
                                         const char* layerName = nullptr) const
        {
            uint32_t extensionCount;
            std::vector<VkExtensionProperties> availableExtensions;
            if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr))
            {
                layerName
                    ? outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName)
                    : outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
                return result;
            }
            if (extensionCount)
            {
                availableExtensions.resize(extensionCount);
                if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data()))
                {
                    outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", int32_t(result));
                    return result;
                }
                for (auto& i : extensionsToCheck)
                {
                    bool found = false;
                    for (auto& j : availableExtensions)
                        if (!strcmp(i, j.extensionName))
                        {
                            found = true;
                            break;
                        }
                    if (!found)
                        i = nullptr;
                }
            }
            else
                for (auto& i : extensionsToCheck)
                    i = nullptr;
            return VK_SUCCESS;
        }

        void InstanceExtensions(const std::vector<const char*>& extensionNames)
        {
            instanceExtensions = extensionNames;
        }

    private:
        VkDebugUtilsMessengerEXT debugMessenger;
        //以下函数用于创建debug messenger
        result_t CreateDebugMessenger()
        {
            static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData)-> VkBool32{
                outStream << std::format("{}\n\n", pCallbackData->pMessage);
                return VK_FALSE;
            };
            VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = DebugUtilsMessengerCallback
            };
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
                reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
                    instance, "vkCreateDebugUtilsMessengerEXT"));
            if (vkCreateDebugUtilsMessenger)
            {
                VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr,
                                                              &debugMessenger);
                if (result)
                    outStream << std::format(
                        "[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n",
                        int32_t(result));
                return result;
            }
            outStream << std::format(
                "[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
            return VK_RESULT_MAX_ENUM;
        }

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

        result_t UseLatestApiVersion()
        {
            if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
                return vkEnumerateInstanceVersion(&apiVersion);
            return VK_SUCCESS;
        }

    private:
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
        std::vector<VkPhysicalDevice> availablePhysicalDevices;

        uint32_t apiVersion = VK_API_VERSION_1_0;
        std::vector<void(*)()> callbacks_createSwapchain;
        std::vector<void(*)()> callbacks_destroySwapchain;
        std::vector<void(*)()> callbacks_createDevice;
        std::vector<void(*)()> callbacks_destroyDevice;

        VkDevice device;
        //有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED（为UINT32_MAX）为队列族索引的默认值
        uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
        uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
        VkQueue queue_graphics;
        VkQueue queue_presentation;
        VkQueue queue_compute;
        //当前取得的交换链图像索引
        uint32_t currentImageIndex = 0;
        std::vector<const char*> deviceExtensions;

        //该函数被DeterminePhysicalDevice(...)调用，用于检查物理设备是否满足所需的队列族类型，并将对应的队列族索引返回到queueFamilyIndices，执行成功时直接将索引写入相应成员变量
        result_t GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue,
                                       bool enableComputeQueue, uint32_t (&queueFamilyIndices)[3])
        {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
            if (!queueFamilyCount)
                return VK_RESULT_MAX_ENUM;
            std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
            auto& [ig, ip, ic] = queueFamilyIndices;
            ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
            for (uint32_t i = 0; i < queueFamilyCount; i++)
            {
                //这三个VkBool32变量指示是否可获取（指应该被获取且能获取）相应队列族索引
                //只在enableGraphicsQueue为true时获取支持图形操作的队列族的索引
                VkBool32 supportGraphics = enableGraphicsQueue && queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT,
                         supportPresentation = false,
                         //只在enableComputeQueue为true时获取支持计算的队列族的索引
                         supportCompute = enableComputeQueue && queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
                //只在创建了window surface时获取支持呈现的队列族的索引
                if (surface)
                    if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation))
                    {
                        outStream << std::format("[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", int32_t(result));
                        return result;
                    }
                //若某队列族同时支持图形操作和计算
                if (supportGraphics && supportCompute)
                {
                    //若需要呈现，最好是三个队列族索引全部相同
                    if (supportPresentation)
                    {
                        ig = ip = ic = i;
                        break;
                    }
                    //除非ig和ic都已取得且相同，否则将它们的值覆写为i，以确保两个队列族索引相同
                    if (ig != ic || ig == VK_QUEUE_FAMILY_IGNORED)
                        ig = ic = i;
                    //如果不需要呈现，那么已经可以break了
                    if (!surface)
                        break;
                }
                //若任何一个队列族索引可以被取得但尚未被取得，将其值覆写为i
                if (supportGraphics && ig == VK_QUEUE_FAMILY_IGNORED)
                    ig = i;
                if (supportPresentation && ip == VK_QUEUE_FAMILY_IGNORED)
                    ip = i;
                if (supportCompute && ic == VK_QUEUE_FAMILY_IGNORED)
                    ic = i;
            }
            if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
                ip == VK_QUEUE_FAMILY_IGNORED && surface ||
                ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
                return VK_RESULT_MAX_ENUM;
            queueFamilyIndex_graphics = ig;
            queueFamilyIndex_presentation = ip;
            queueFamilyIndex_compute = ic;
            return VK_SUCCESS;
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
        result_t GetPhysicalDevices()
        {
            uint32_t deviceCount;
            if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: {}\n", int32_t(result));
                return result;
            }
            if (!deviceCount)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"),
                    abort();
            availablePhysicalDevices.resize(deviceCount);
            VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
            if (result)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
            return result;
        }

        //该函数用于指定所用物理设备并调用GetQueueFamilyIndices(...)取得队列族索引
        result_t DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true)
        {
            //定义一个特殊值用于标记一个队列族索引已被找过但未找到
            static constexpr uint32_t notFound = INT32_MAX; //== VK_QUEUE_FAMILY_IGNORED & INT32_MAX
            //定义队列族索引组合的结构体
            struct queueFamilyIndexCombination
            {
                uint32_t graphics = VK_QUEUE_FAMILY_IGNORED;
                uint32_t presentation = VK_QUEUE_FAMILY_IGNORED;
                uint32_t compute = VK_QUEUE_FAMILY_IGNORED;
            };
            //queueFamilyIndexCombinations用于为各个物理设备保存一份队列族索引组合
            static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(availablePhysicalDevices.size());
            auto& [ig, ip, ic] = queueFamilyIndexCombinations[deviceIndex];

            //如果有任何队列族索引已被找过但未找到，返回VK_RESULT_MAX_ENUM
            if (ig == notFound && enableGraphicsQueue || ip == notFound && surface || ic == notFound && enableComputeQueue)
                return VK_RESULT_MAX_ENUM;

            //如果有任何队列族索引应被获取但还未被找过
            if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue || ip == VK_QUEUE_FAMILY_IGNORED && surface || ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
            {
                uint32_t indices[3];
                VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex], enableGraphicsQueue, enableComputeQueue, indices);
                //若GetQueueFamilyIndices(...)返回VK_SUCCESS或VK_RESULT_MAX_ENUM（vkGetPhysicalDeviceSurfaceSupportKHR(...)执行成功但没找齐所需队列族），
                //说明对所需队列族索引已有结论，保存结果到queueFamilyIndexCombinations[deviceIndex]中相应变量
                //应被获取的索引若仍为VK_QUEUE_FAMILY_IGNORED，说明未找到相应队列族，VK_QUEUE_FAMILY_IGNORED（~0u）与INT32_MAX做位与得到的数值等于notFound
                if (result == VK_SUCCESS || result == VK_RESULT_MAX_ENUM)
                {
                    if (enableGraphicsQueue)
                        ig = indices[0] & INT32_MAX;
                    if (surface)
                        ip = indices[1] & INT32_MAX;
                    if (enableComputeQueue)
                        ic = indices[2] & INT32_MAX;
                }
                //如果GetQueueFamilyIndices(...)执行失败，return
                if (result)
                    return result;
            }

            //若以上两个if分支皆不执行，则说明所需的队列族索引皆已被获取，从queueFamilyIndexCombinations[deviceIndex]中取得索引
            else
            {
                queueFamilyIndex_graphics = enableGraphicsQueue
                                                ? ig
                                                : VK_QUEUE_FAMILY_IGNORED;
                queueFamilyIndex_presentation = surface
                                                    ? ip
                                                    : VK_QUEUE_FAMILY_IGNORED;
                queueFamilyIndex_compute = enableComputeQueue
                                               ? ic
                                               : VK_QUEUE_FAMILY_IGNORED;
            }
            physicalDevice = availablePhysicalDevices[deviceIndex];
            return VK_SUCCESS;
        }

        //该函数用于创建逻辑设备，并取得队列
        result_t CreateDevice(VkDeviceCreateFlags flags = 0)
        {
            float queuePriority = 1.f;
            VkDeviceQueueCreateInfo queueCreateInfos[3] = {
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
                },
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
                },
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority
                }
            };
            uint32_t queueCreateInfoCount = 0;
            if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
                queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
            if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
                queueFamilyIndex_presentation != queueFamilyIndex_graphics)
                queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
            if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
                queueFamilyIndex_compute != queueFamilyIndex_graphics &&
                queueFamilyIndex_compute != queueFamilyIndex_presentation)
                queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;
            VkPhysicalDeviceFeatures physicalDeviceFeatures;
            vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
            VkDeviceCreateInfo deviceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .flags = flags,
                .queueCreateInfoCount = queueCreateInfoCount,
                .pQueueCreateInfos = queueCreateInfos,
                .enabledExtensionCount = uint32_t(deviceExtensions.size()),
                .ppEnabledExtensionNames = deviceExtensions.data(),
                .pEnabledFeatures = &physicalDeviceFeatures
            };
            if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", int32_t(result));
                return result;
            }
            if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
                vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
            if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED)
                vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
            if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED)
                vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
            //输出所用的物理设备名称
            outStream << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
            //for (auto& i : callbacks_createDevice)
            //  i();
            return VK_SUCCESS;
        }

        //以下函数用于创建逻辑设备失败后
        result_t CheckDeviceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const
        {
            uint32_t extensionCount;
            std::vector<VkExtensionProperties> availableExtensions;
            if (VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &extensionCount, nullptr))
            {
                layerName
                    ? outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of device extensions!\nLayer name:{}\n", layerName)
                    : outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of device extensions!\n");
                return result;
            }
            if (extensionCount)
            {
                availableExtensions.resize(extensionCount);
                if (VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &extensionCount, availableExtensions.data()))
                {
                    outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate device extension properties!\nError code: {}\n", int32_t(result));
                    return result;
                }
                for (auto& i : extensionsToCheck)
                {
                    bool found = false;
                    for (auto& j : availableExtensions)
                        if (!strcmp(i, j.extensionName))
                        {
                            found = true;
                            break;
                        }
                    if (!found)
                        i = nullptr; //If a required extension isn't available, set it to nullptr
                }
            }
            else
                for (auto& i : extensionsToCheck)
                    i = nullptr;
            return VK_SUCCESS;
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
        result_t CreateSwapchain_Internal()
        {
            if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", int32_t(result));
                return result;
            }

            //获取交换连图像
            uint32_t swapchainImageCount;
            if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n", int32_t(result));
                return result;
            }
            swapchainImages.resize(swapchainImageCount);
            if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", int32_t(result));
                return result;
            }

            //创建image view
            swapchainImageViews.resize(swapchainImageCount);
            VkImageViewCreateInfo imageViewCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchainCreateInfo.imageFormat,
                //.components = {},//四个成员皆为VK_COMPONENT_SWIZZLE_IDENTITY
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };
            for (size_t i = 0; i < swapchainImageCount; i++)
            {
                imageViewCreateInfo.image = swapchainImages[i];
                if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]))
                {
                    outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n", int32_t(result));
                    return result;
                }
            }
            return VK_SUCCESS;
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

        uint32_t CurrentImageIndex() const
        {
            return currentImageIndex;
        }

        const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const
        {
            return swapchainCreateInfo;
        }

        uint32_t SwapchainImageCount() const
        {
            return uint32_t(swapchainImages.size());
        }

        result_t GetSurfaceFormats()
        {
            uint32_t surfaceFormatCount;
            if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n", int32_t(result));
                return result;
            }
            if (!surfaceFormatCount)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"),
                    abort();
            availableSurfaceFormats.resize(surfaceFormatCount);
            VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
            if (result)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
        {
            bool formatIsAvailable = false;
            if (!surfaceFormat.format)
            {
                //如果格式未指定，只匹配色彩空间，图像格式有啥就用啥
                for (auto& i : availableSurfaceFormats)
                    if (i.colorSpace == surfaceFormat.colorSpace)
                    {
                        swapchainCreateInfo.imageFormat = i.format;
                        swapchainCreateInfo.imageColorSpace = i.colorSpace;
                        formatIsAvailable = true;
                        break;
                    }
            }
            else
            //否则匹配格式和色彩空间
                for (auto& i : availableSurfaceFormats)
                    if (i.format == surfaceFormat.format && i.colorSpace == surfaceFormat.colorSpace)
                    {
                        swapchainCreateInfo.imageFormat = i.format;
                        swapchainCreateInfo.imageColorSpace = i.colorSpace;
                        formatIsAvailable = true;
                        break;
                    }
            //如果没有符合的格式，恰好有个语义相符的错误代码
            if (!formatIsAvailable)
                return VK_ERROR_FORMAT_NOT_SUPPORTED;
            //如果交换链已存在，调用RecreateSwapchain()重建交换链
            if (swapchain)
                return RecreateSwapchain();
            return VK_SUCCESS;
        }

        //该函数用于创建交换链
        result_t CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0)
        {
            //VkSurfaceCapabilitiesKHR相关的参数
            VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
            if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
                return result;
            }
            //指定图像数量
            swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
            //指定图像大小
            swapchainCreateInfo.imageExtent =
                surfaceCapabilities.currentExtent.width == -1
                    ? VkExtent2D{
                        glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
                        glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)
                    }
                    : surfaceCapabilities.currentExtent;
            //swapchainCreateInfo.imageArrayLayers = 1;//跟其他不需要判断的参数一起扔后面去
            //指定变换方式
            swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
            //指定处理透明通道的方式
            if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
                swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
            else
                for (size_t i = 0; i < 4; i++)
                    if (surfaceCapabilities.supportedCompositeAlpha & 1 << i)
                    {
                        swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & 1 << i);
                        break;
                    }
            //指定图像用途
            swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            else
                outStream << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");

            //指定图像格式
            if (!availableSurfaceFormats.size())
                if (VkResult result = GetSurfaceFormats())
                    return result;
            if (!swapchainCreateInfo.imageFormat)
                //用&&操作符来短路执行
                if (SetSurfaceFormat({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) &&
                    SetSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}))
                {
                    //如果找不到上述图像格式和色彩空间的组合，那只能有什么用什么，采用availableSurfaceFormats中的第一组
                    swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
                    swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
                    outStream << std::format("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
                }
            //指定呈现模式
            uint32_t surfacePresentModeCount;
            if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n", int32_t(result));
                return result;
            }
            if (!surfacePresentModeCount)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"),
                    abort();
            std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
            if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data()))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", int32_t(result));
                return result;
            }
            //Set present mode to mailbox if available and necessary
            swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            if (!limitFrameRate)
                for (size_t i = 0; i < surfacePresentModeCount; i++)
                    if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                    {
                        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                        break;
                    }

            //剩余参数
            swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapchainCreateInfo.flags = flags;
            swapchainCreateInfo.surface = surface;
            swapchainCreateInfo.imageArrayLayers = 1;
            swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainCreateInfo.clipped = VK_TRUE;

            //创建交换链
            if (VkResult result = CreateSwapchain_Internal())
                return result;
            //执行回调函数
            for (auto& i : callbacks_createSwapchain)
                i();
            return VK_SUCCESS;
        }


        //该函数用于重建交换链
        result_t RecreateSwapchain()
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
            if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
                return result;
            }
            if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
                return VK_SUBOPTIMAL_KHR;
            swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
            swapchainCreateInfo.oldSwapchain = swapchain;
            VkResult result = vkQueueWaitIdle(queue_graphics);
            if (result == VK_SUCCESS && queue_graphics != queue_presentation)
                result = vkQueueWaitIdle(queue_presentation);
            if (result)
            {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n", int32_t(result));
                return result;
            }
            //销毁旧交换链相关对象
            for (auto& i : callbacks_destroySwapchain)
                i();
            for (auto& i : swapchainImageViews)
                if (i)
                    vkDestroyImageView(device, i, nullptr);
            swapchainImageViews.resize(0);
            //创建新交换链及与之相关的对象
            if (result == CreateSwapchain_Internal())
                return result;
            for (auto& i : callbacks_createSwapchain)
                i();
            return VK_SUCCESS;
        }

        //该函数用于获取交换链图像索引到currentImageIndex，以及在需要重建交换链时调用RecreateSwapchain()、重建交换链后销毁旧交换链
        result_t SwapImage(VkSemaphore semaphore_imageIsAvailable)
        {
            //销毁旧交换链（若存在）
            if (swapchainCreateInfo.oldSwapchain &&
                swapchainCreateInfo.oldSwapchain != swapchain)
            {
                vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
                swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
            }
            //获取交换链图像索引
            while (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, VK_NULL_HANDLE, &currentImageIndex))
                switch (result)
                {
                case VK_SUBOPTIMAL_KHR:
                case VK_ERROR_OUT_OF_DATE_KHR:
                    if (VkResult result = RecreateSwapchain())
                        return result;
                    break; //注意重建交换链后仍需要获取图像，通过break递归，再次执行while的条件判定语句
                default:
                    outStream << std::format("[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: {}\n", int32_t(result));
                    return result;
                }
            return VK_SUCCESS;
        }

        //该函数用于将命令缓冲区提交到用于图形的队列
        result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer,
                                              VkSemaphore semaphore_imageIsAvailable = VK_NULL_HANDLE, VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE,
                                              VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const
        {
            VkSubmitInfo submitInfo = {
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer
            };
            if (semaphore_imageIsAvailable)
                submitInfo.waitSemaphoreCount = 1,
                    submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable,
                    submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
            if (semaphore_renderingIsOver)
                submitInfo.signalSemaphoreCount = 1,
                    submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
            return SubmitCommandBuffer_Graphics(submitInfo, fence);
        }

        //该函数用于在渲染循环中将命令缓冲区提交到图形队列的常见情形
        result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const
        {
            VkSubmitInfo submitInfo = {
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer
            };
            return SubmitCommandBuffer_Graphics(submitInfo, fence);
        }

        //该函数用于将命令缓冲区提交到用于图形的队列，且只使用栅栏的常见情形
        result_t SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkResult result = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
            if (result)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
            return result;
        }

        //该函数用于将命令缓冲区提交到用于计算的队列
        result_t SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const
        {
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkResult result = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
            if (result)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
            return result;
        }

        //该函数用于将命令缓冲区提交到用于计算的队列，且只使用栅栏的常见情形
        result_t SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const
        {
            VkSubmitInfo submitInfo = {
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer
            };
            return SubmitCommandBuffer_Compute(submitInfo, fence);
        }

        result_t SubmitCommandBuffer_Presentation(VkCommandBuffer commandBuffer,
                                                  VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkSemaphore semaphore_ownershipIsTransfered = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE) const
        {
            static constexpr VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer
            };
            if (semaphore_renderingIsOver)
                submitInfo.waitSemaphoreCount = 1,
                    submitInfo.pWaitSemaphores = &semaphore_renderingIsOver,
                    submitInfo.pWaitDstStageMask = &waitDstStage;
            if (semaphore_ownershipIsTransfered)
                submitInfo.signalSemaphoreCount = 1,
                    submitInfo.pSignalSemaphores = &semaphore_ownershipIsTransfered;
            VkResult result = vkQueueSubmit(queue_presentation, 1, &submitInfo, fence);
            if (result)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the presentation command buffer!\nError code: {}\n", int32_t(result));
            return result;
        }

        void CmdTransferImageOwnership(VkCommandBuffer commandBuffer) const
        {
            //这个VkImageMemoryBarrier可以跟上文的一模一样
            VkImageMemoryBarrier imageMemoryBarrier_g2p = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = 0, //因为vkCmdPipelineBarrier(...)的参数中dstStage是BOTTOM_OF_PIPE，不需要dstAccessMask
                .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, //内存布局已经在渲染通道结束时转换（这是下一节的内容），此处oldLayout和newLayout相同，不发生转变
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = queueFamilyIndex_graphics,
                .dstQueueFamilyIndex = queueFamilyIndex_presentation,
                .image = swapchainImages[currentImageIndex],
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
            };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_g2p);
        }

        result_t PresentImage(VkPresentInfoKHR& presentInfo)
        {
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo))
            {
            case VK_SUCCESS:
                return VK_SUCCESS;
            case VK_SUBOPTIMAL_KHR:
            case VK_ERROR_OUT_OF_DATE_KHR:
                return RecreateSwapchain();
            default:
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: {}\n", int32_t(result));
                return result;
            }
        }

        //该函数用于在渲染循环中呈现图像的常见情形
        result_t PresentImage(VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE)
        {
            VkPresentInfoKHR presentInfo = {
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &currentImageIndex
            };
            if (semaphore_renderingIsOver)
                presentInfo.waitSemaphoreCount = 1,
                    presentInfo.pWaitSemaphores = &semaphore_renderingIsOver;
            return PresentImage(presentInfo);
        }

        //Getter
        uint32_t ApiVersion() const
        {
            return apiVersion;
        }

        void AddCallback_CreateSwapchain(void (*function)())
        {
            callbacks_createSwapchain.push_back(function);
        }

        void AddCallback_DestroySwapchain(void (*function)())
        {
            callbacks_destroySwapchain.push_back(function);
        }

        void AddCallback_CreateDevice(void (*function)())
        {
            callbacks_createDevice.push_back(function);
        }

        void AddCallback_DestroyDevice(void (*function)())
        {
            callbacks_destroyDevice.push_back(function);
        }

        //该函数用于等待逻辑设备空闲
        result_t WaitIdle() const
        {
            VkResult result = vkDeviceWaitIdle(device);
            if (result)
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n", int32_t(result));
            return result;
        }

        //该函数用于重建逻辑设备
        result_t RecreateDevice(VkDeviceCreateFlags flags = 0)
        {
            if (VkResult result = WaitIdle())
                return result;
            if (swapchain)
            {
                //调用销毁交换链时的回调函数
                for (auto& i : callbacks_destroySwapchain)
                    i();
                //销毁交换链图像的image view
                for (auto& i : swapchainImageViews)
                    if (i)
                        vkDestroyImageView(device, i, nullptr);
                swapchainImageViews.resize(0);
                //销毁交换链
                vkDestroySwapchainKHR(device, swapchain, nullptr);
                //重置交换链handle
                swapchain = VK_NULL_HANDLE;
                //重置交换链创建信息
                swapchainCreateInfo = {};
            }
            for (auto& i : callbacks_destroyDevice)
                i();
            if (device) //防函数执行失败后再次调用时发生重复销毁
                vkDestroyDevice(device, nullptr),
                    device = VK_NULL_HANDLE;
            return CreateDevice(flags);
        }
    };

    inline graphicsBase graphicsBase::singleton;

    class semaphore
    {
        VkSemaphore handle = VK_NULL_HANDLE;

    public:
        //semaphore() = default;
        semaphore(VkSemaphoreCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        semaphore(/*reserved for future use*/)
        {
            Create();
        }

        semaphore(semaphore&& other) noexcept
        {
            MoveHandle;
        }

        ~semaphore()
        {
            DestroyHandleBy(vkDestroySemaphore);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkSemaphoreCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkResult result = vkCreateSemaphore(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Create(/*reserved for future use*/)
        {
            VkSemaphoreCreateInfo createInfo = {};
            return Create(createInfo);
        }
    };

    class fence
    {
        VkFence handle = VK_NULL_HANDLE;

    public:
        //fence() = default;
        fence(VkFenceCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        fence(VkFenceCreateFlags flags = 0)
        {
            Create(flags);
        }

        fence(fence&& other) noexcept
        {
            MoveHandle;
        }

        ~fence()
        {
            DestroyHandleBy(vkDestroyFence);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        result_t Wait() const
        {
            VkResult result = vkWaitForFences(graphicsBase::Base().Device(), 1, &handle, false, UINT64_MAX);
            if (result)
                outStream << std::format("[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Reset() const
        {
            VkResult result = vkResetFences(graphicsBase::Base().Device(), 1, &handle);
            if (result)
                outStream << std::format("[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t WaitAndReset() const
        {
            VkResult result = Wait();
            result || (result = Reset());
            return result;
        }

        result_t Status() const
        {
            VkResult result = vkGetFenceStatus(graphicsBase::Base().Device(), handle);
            if (result < 0)
                outStream << std::format("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n", int32_t(result));
            return result;
        }

        //Non-const Function
        result_t Create(VkFenceCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkResult result = vkCreateFence(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Create(VkFenceCreateFlags flags = 0)
        {
            VkFenceCreateInfo createInfo = {
                .flags = flags
            };
            return Create(createInfo);
        }
    };

    class shaderModule
    {
        VkShaderModule handle = VK_NULL_HANDLE;

    public:
        shaderModule() = default;

        shaderModule(VkShaderModuleCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        shaderModule(const char* filepath /*reserved for future use*/)
        {
            Create(filepath);
        }

        shaderModule(size_t codeSize, const uint32_t* pCode /*reserved for future use*/)
        {
            Create(codeSize, pCode);
        }

        shaderModule(shaderModule&& other) noexcept
        {
            MoveHandle;
        }

        ~shaderModule()
        {
            DestroyHandleBy(vkDestroyShaderModule);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const
        {
            return {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //sType
                nullptr, //pNext
                0, //flags
                stage, //stage
                handle, //module
                entry, //pName
                nullptr //pSpecializationInfo
            };
        }

        //Non-const Function
        result_t Create(VkShaderModuleCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            VkResult result = vkCreateShaderModule(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ shader ] ERROR\nFailed to create a shader module!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Create(const char* filepath /*reserved for future use*/)
        {
            std::ifstream file(filepath, std::ios::ate | std::ios::binary);
            if (!file)
            {
                outStream << std::format("[ shader ] ERROR\nFailed to open the file: {}\n", filepath);
                return VK_RESULT_MAX_ENUM; //No proper VkResult enum value, don't use VK_ERROR_UNKNOWN
            }
            size_t fileSize = size_t(file.tellg());
            std::vector<uint32_t> binaries(fileSize / 4);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(binaries.data()), fileSize);
            file.close();
            return Create(fileSize, binaries.data());
        }

        result_t Create(size_t codeSize, const uint32_t* pCode /*reserved for future use*/)
        {
            VkShaderModuleCreateInfo createInfo = {
                .codeSize = codeSize,
                .pCode = pCode
            };
            return Create(createInfo);
        }
    };

    class pipelineLayout
    {
        VkPipelineLayout handle = VK_NULL_HANDLE;

    public:
        pipelineLayout() = default;

        pipelineLayout(VkPipelineLayoutCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        pipelineLayout(pipelineLayout&& other) noexcept
        {
            MoveHandle;
        }

        ~pipelineLayout()
        {
            DestroyHandleBy(vkDestroyPipelineLayout);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkPipelineLayoutCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            VkResult result = vkCreatePipelineLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ pipelineLayout ] ERROR\nFailed to create a pipeline layout!\nError code: {}\n", int32_t(result));
            return result;
        }
    };

    class pipeline
    {
        VkPipeline handle = VK_NULL_HANDLE;

    public:
        pipeline() = default;

        pipeline(VkGraphicsPipelineCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        pipeline(VkComputePipelineCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        pipeline(pipeline&& other) noexcept
        {
            MoveHandle;
        }

        ~pipeline()
        {
            DestroyHandleBy(vkDestroyPipeline);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkGraphicsPipelineCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            VkResult result = vkCreateGraphicsPipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ pipeline ] ERROR\nFailed to create a graphics pipeline!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Create(VkComputePipelineCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            VkResult result = vkCreateComputePipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ pipeline ] ERROR\nFailed to create a compute pipeline!\nError code: {}\n", int32_t(result));
            return result;
        }
    };

    class renderPass
    {
        VkRenderPass handle = VK_NULL_HANDLE;

    public:
        renderPass() = default;

        renderPass(VkRenderPassCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        renderPass(renderPass&& other) noexcept
        {
            MoveHandle;
        }

        ~renderPass()
        {
            DestroyHandleBy(vkDestroyRenderPass);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        void CmdBegin(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
        {
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = handle;
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
        }

        void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, arrayRef<const VkClearValue> clearValues = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
        {
            VkRenderPassBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = handle,
                .framebuffer = framebuffer,
                .renderArea = renderArea,
                .clearValueCount = uint32_t(clearValues.Count()),
                .pClearValues = clearValues.Pointer()
            };
            vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
        }

        void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
        {
            vkCmdNextSubpass(commandBuffer, subpassContents);
        }

        void CmdEnd(VkCommandBuffer commandBuffer) const
        {
            vkCmdEndRenderPass(commandBuffer);
        }

        //Non-const Function
        result_t Create(VkRenderPassCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", int32_t(result));
            return result;
        }
    };

    class framebuffer
    {
        VkFramebuffer handle = VK_NULL_HANDLE;

    public:
        framebuffer() = default;

        framebuffer(VkFramebufferCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        framebuffer(framebuffer&& other) noexcept
        {
            MoveHandle;
        }

        ~framebuffer()
        {
            DestroyHandleBy(vkDestroyFramebuffer);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Non-const Function
        result_t Create(VkFramebufferCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            VkResult result = vkCreateFramebuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", int32_t(result));
            return result;
        }
    };

    class commandBuffer
    {
        friend class commandPool;
        VkCommandBuffer handle = VK_NULL_HANDLE;

    public:
        commandBuffer() = default;

        commandBuffer(commandBuffer&& other) noexcept
        {
            MoveHandle;
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        result_t Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const
        {
            inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
            VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = usageFlags,
                .pInheritanceInfo = &inheritanceInfo
            };
            VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
            if (result)
                outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Begin(VkCommandBufferUsageFlags usageFlags = 0) const
        {
            VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = usageFlags,
            };
            VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
            if (result)
                outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t End() const
        {
            VkResult result = vkEndCommandBuffer(handle);
            if (result)
                outStream << std::format("[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {}\n", int32_t(result));
            return result;
        }
    };

    class commandPool
    {
        VkCommandPool handle = VK_NULL_HANDLE;

    public:
        commandPool() = default;

        commandPool(VkCommandPoolCreateInfo& createInfo)
        {
            Create(createInfo);
        }

        commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
        {
            Create(queueFamilyIndex, flags);
        }

        commandPool(commandPool&& other) noexcept
        {
            MoveHandle;
        }

        ~commandPool()
        {
            DestroyHandleBy(vkDestroyCommandPool);
        }

        //Getter
        DefineHandleTypeOperator;
        DefineAddressFunction;
        //Const Function
        result_t AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const
        {
            VkCommandBufferAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = handle,
                .level = level,
                .commandBufferCount = uint32_t(buffers.Count())
            };
            VkResult result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
            if (result)
                outStream << std::format("[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t AllocateBuffers(arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const
        {
            return AllocateBuffers(
                {&buffers[0].handle, buffers.Count()},
                level);
        }

        void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const
        {
            vkFreeCommandBuffers(graphicsBase::Base().Device(), handle, buffers.Count(), buffers.Pointer());
            memset(buffers.Pointer(), 0, buffers.Count() * sizeof(VkCommandBuffer));
        }

        void FreeBuffers(arrayRef<commandBuffer> buffers) const
        {
            FreeBuffers({&buffers[0].handle, buffers.Count()});
        }

        //Non-const Function
        result_t Create(VkCommandPoolCreateInfo& createInfo)
        {
            createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            VkResult result = vkCreateCommandPool(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
            if (result)
                outStream << std::format("[ commandPool ] ERROR\nFailed to create a command pool!\nError code: {}\n", int32_t(result));
            return result;
        }

        result_t Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
        {
            VkCommandPoolCreateInfo createInfo = {
                .flags = flags,
                .queueFamilyIndex = queueFamilyIndex
            };
            return Create(createInfo);
        }
    };
}
