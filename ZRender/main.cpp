#include "GlfwGeneral.hpp"

int main()
{
    // 先创建 GLFW 窗口，并在这一课中顺带完成实例、设备与交换链初始化。
    if (!InitializeWindow(vulkan::defaultWindowSize))
        return -1;

    // 在真正开始渲染前，先保留一个空循环框架。
    while (!glfwWindowShouldClose(pWindow))
    {
        // 渲染流程会从后续章节开始逐步填充。

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}
