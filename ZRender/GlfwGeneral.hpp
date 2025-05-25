#pragma once

#include "VKBase.h"

// 让 GLFW 自动包含 Vulkan 相关声明，后续创建 surface 时会直接用到。
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// 直接链接 GLFW 静态库，方便 Visual Studio 工程使用。
#pragma comment(lib, "glfw3.lib")

// 保存主窗口指针，后续各章节都会复用。
inline GLFWwindow* pWindow = nullptr;

// 保存主显示器指针，方便后续切换全屏与窗口模式。
inline GLFWmonitor* pMonitor = nullptr;

// 统一窗口标题，后续会在标题上叠加 FPS。
inline constexpr char windowTitle[] = "ZRender";

bool InitializeWindow(
    VkExtent2D size,
    bool fullScreen = false,
    bool isResizable = true)
{
    // 第一步先初始化 GLFW 运行时。
    if (!glfwInit())
    {
        vulkan::outStream << "[ InitializeWindow ] ERROR\n";
        vulkan::outStream << "Failed to initialize GLFW.\n";
        return false;
    }

    // 明确告诉 GLFW：窗口的图形上下文由 Vulkan 管理，而不是 OpenGL。
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // 让调用方决定窗口是否允许拖拽缩放。
    glfwWindowHint(GLFW_RESIZABLE, isResizable ? GLFW_TRUE : GLFW_FALSE);

    // 记录主显示器，后续全屏模式会用到。
    pMonitor = glfwGetPrimaryMonitor();

    // 查询当前显示器的视频模式。
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);

    // 根据 fullScreen 参数决定创建普通窗口还是全屏窗口。
    pWindow = fullScreen
                  ? glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr)
                  : glfwCreateWindow(static_cast<int>(size.width), static_cast<int>(size.height), windowTitle, nullptr, nullptr);

    // 如果窗口创建失败，就清理 GLFW 并返回失败。
    if (!pWindow)
    {
        vulkan::outStream << "[ InitializeWindow ] ERROR\n";
        vulkan::outStream << "Failed to create a GLFW window.\n";
        glfwTerminate();
        return false;
    }

    // 到这里说明窗口已经创建成功。
    return true;
}

void TerminateWindow()
{
    // 当前阶段只需要释放 GLFW 即可。
    glfwTerminate();
}

void MakeWindowFullScreen()
{
    // 读取当前显示器模式，切回真正的全屏窗口。
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    glfwSetWindowMonitor(pWindow, pMonitor, 0, 0, pMode->width, pMode->height, pMode->refreshRate);
}

void MakeWindowWindowed(VkOffset2D position, VkExtent2D size)
{
    // 从全屏切回窗口模式时，位置和尺寸都由调用方决定。
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    glfwSetWindowMonitor(
        pWindow,
        nullptr,
        position.x,
        position.y,
        static_cast<int>(size.width),
        static_cast<int>(size.height),
        pMode->refreshRate);
}

void TitleFps()
{
    // 记录上一次统计 FPS 的时间点。
    static double time0 = glfwGetTime();

    // 记录当前时间点。
    const double time1 = glfwGetTime();

    // 统计间隔时长。
    const double deltaTime = time1 - time0;

    // 统计这一秒内累计的帧数。
    static int frameCount = 0;
    ++frameCount;

    // 每满一秒更新一次窗口标题，避免频繁设置标题。
    if (deltaTime >= 1.0)
    {
        // 用字符串流拼出最终标题文本。
        std::stringstream titleStream;

        // 保留一位小数，方便观察帧率变化。
        titleStream.precision(1);

        // 生成“标题 + FPS”的显示文本。
        titleStream << windowTitle << "    " << std::fixed << frameCount / deltaTime << " FPS";

        // 把结果真正写回窗口标题。
        glfwSetWindowTitle(pWindow, titleStream.str().c_str());

        // 更新时间基准，开始下一轮统计。
        time0 = time1;

        // 重置帧计数器。
        frameCount = 0;
    }
}
