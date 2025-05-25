#include "GlfwGeneral.hpp"

int main()
{
    // 先创建一个 1280x720 的 GLFW 窗口。
    if (!InitializeWindow(vulkan::defaultWindowSize))
    {
        return -1;
    }

    // 当用户没有关闭窗口时，就持续执行消息循环。
    while (!glfwWindowShouldClose(pWindow))
    {
        // 处理系统消息，比如鼠标、键盘和窗口事件。
        glfwPollEvents();

        // 持续刷新标题栏中的 FPS 显示。
        TitleFps();
    }

    // 退出前释放窗口系统资源。
    TerminateWindow();

    return 0;
}
