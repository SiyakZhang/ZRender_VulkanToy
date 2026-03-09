#pragma once
#include "VKBase.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#pragma comment(lib, "glfw3dll.lib") //改为链接 GLFW 的导入库，避免把 release 静态库里的 CRT 设定混进当前工程

//窗口的指针，全局变量自动初始化为NULL
inline GLFWwindow* pWindow;
//显示器信息的指针
inline GLFWmonitor* pMonitor;
//窗口标题
inline auto windowTitle = "ZRender";

auto PreInitialization_EnableSrgb()
{
    // 用静态布尔值保存“启动前是否要求 sRGB 交换链”的开关。
    static bool enableSrgb = false;

    // 一旦调用这个函数，就把开关置为 true。
    enableSrgb = true;

    // 返回一个无捕获 lambda，后面可通过默认构造同类型对象来查询该开关。
    return [] { return enableSrgb; };
}

auto PreInitialization_TrySetColorSpaceByOrder(arrayRef<const VkColorSpaceKHR> colorSpaces)
{
    // 这里把用户想尝试的色彩空间顺序缓存下来，供 InitializeWindow 在真正创建设备后再读取。
    static std::unique_ptr<VkColorSpaceKHR[]> pColorSpaces;

    // 额外多分配 1 个元素，保持数组尾部为 0，便于后面用 while(*pColorSpaces) 遍历。
    pColorSpaces = std::make_unique<VkColorSpaceKHR[]>(colorSpaces.Count() + 1);

    // 把调用方给出的色彩空间顺序复制进静态缓存。
    std::memcpy(pColorSpaces.get(), colorSpaces.Pointer(), sizeof(VkColorSpaceKHR) * colorSpaces.Count());

    // 同样返回一个无捕获 lambda，便于后续默认构造后读取缓存指针。
    return []() -> const VkColorSpaceKHR* { return pColorSpaces.get(); };
}

bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true)
{
    using namespace vulkan;

    if (!glfwInit())
    {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, isResizable);
    pMonitor = glfwGetPrimaryMonitor();
    pWindow = fullScreen
                  ? glfwCreateWindow(size.width, size.height, windowTitle, pMonitor, nullptr)
                  : glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
    if (!pWindow)
    {
        std::cout << std::format("[ InitializeWindow ]\nFailed to create a glfw window!\n");
        glfwTerminate();
        return false;
    }

#ifdef _WIN32
    graphicsBase::Base().AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    graphicsBase::Base().AddInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
    uint32_t extensionCount = 0;
    const char** extensionNames;
    extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
        glfwTerminate();
        return false;
    }
    for (size_t i = 0; i < extensionCount; i++)
        graphicsBase::Base().AddInstanceExtension(extensionNames[i]);
#endif

    // 若用户在初始化前请求了特殊色彩空间，就额外启用 swapchain colorspace 扩展。
    const auto queryRequestedColorSpaces = decltype(PreInitialization_TrySetColorSpaceByOrder({})){};
    const VkColorSpaceKHR* pRequestedColorSpaces = queryRequestedColorSpaces();
    if (pRequestedColorSpaces)
        graphicsBase::Base().AddInstanceExtension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);

    graphicsBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    //在创建window surface前创建Vulkan实例
    graphicsBase::Base().UseLatestApiVersion();
    if (graphicsBase::Base().CreateInstance())
        return false;

    //创建window surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (VkResult result = glfwCreateWindowSurface(graphicsBase::Base().Instance(), pWindow, nullptr, &surface))
    {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", int32_t(result));
        glfwTerminate();
        return false;
    }
    graphicsBase::Base().Surface(surface);

    //通过用||操作符短路执行来省去几行
    if ( //获取物理设备，并使用列表中的第一个物理设备，这里不考虑以下任意函数失败后更换物理设备的情况
        graphicsBase::Base().GetPhysicalDevices() ||
        //一个true一个false，暂时不需要计算用的队列
        graphicsBase::Base().DeterminePhysicalDevice(0, true, false) ||
        //创建逻辑设备
        graphicsBase::Base().CreateDevice())
        return false;

    // 先把 surface 支持的“图像格式 + 色彩空间”列表拉出来，供后面挑选交换链格式。
    if (graphicsBase::Base().GetSurfaceFormats())
        return false;

    if (pRequestedColorSpaces)
    {
        // 按调用方给出的优先级依次尝试色彩空间。
        VkResult result_setColorSpace = VK_SUCCESS;

        while (*pRequestedColorSpaces)
        {
            // 这里只指定色彩空间，不指定格式；实际可用格式交给 surface 自己选。
            result_setColorSpace = graphicsBase::Base().SetSurfaceFormat({VK_FORMAT_UNDEFINED, *pRequestedColorSpaces++});
            if (result_setColorSpace == VK_SUCCESS)
                break;
        }

        if (result_setColorSpace)
            std::cout << std::format("[ InitializeWindow ] WARNING\nFailed to satisfy the requirement of color space!\n");
    }

    // 如果还没选到交换链格式，就再看是否要求启用 sRGB 后缀格式。
    const auto queryEnableSrgb = decltype(PreInitialization_EnableSrgb()){};
    if (!graphicsBase::Base().SwapchainCreateInfo().imageFormat && queryEnableSrgb())
        if (graphicsBase::Base().SetSurfaceFormat({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) &&
            graphicsBase::Base().SetSurfaceFormat({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}))
            std::cout << std::format("[ InitializeWindow ] WARNING\nFailed to enable sRGB!\n");

    if (graphicsBase::Base().CreateSwapchain(limitFrameRate))
        return false;
    return true;
}

void TerminateWindow()
{
    vulkan::graphicsBase::Base().WaitIdle();
    glfwTerminate();
}

void TitleFps()
{
    static double time0 = glfwGetTime();
    static double time1;
    static double dt;
    static int dframe = -1;
    static std::stringstream info;
    time1 = glfwGetTime();
    dframe++;
    if ((dt = time1 - time0) >= 1)
    {
        info.precision(1);
        info << windowTitle << "    " << std::fixed << dframe / dt << " FPS";
        glfwSetWindowTitle(pWindow, info.str().c_str());
        info.str(""); //别忘了在设置完窗口标题后清空所用的stringstream
        time0 = time1;
        dframe = 0;
    }
}

void MakeWindowFullScreen()
{
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    glfwSetWindowMonitor(pWindow, pMonitor, 0, 0, pMode->width, pMode->height, pMode->refreshRate);
}

void MakeWindowWindowed(VkOffset2D position, VkExtent2D size)
{
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    glfwSetWindowMonitor(pWindow, nullptr, position.x, position.y, size.width, size.height, pMode->refreshRate);
}
