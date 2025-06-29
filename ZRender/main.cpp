#include "GlfwGeneral.hpp"

using namespace vulkan;

int main()
{
    // 先完成窗口、实例、设备与交换链初始化。
    if (!InitializeWindow(defaultWindowSize))
        return -1;

    // 先创建一个“初始为已完成状态”的栅栏，保证第一帧不用卡在等待上。
    fence fence(VK_FENCE_CREATE_SIGNALED_BIT);

    // 获取交换链图像成功后，呈现引擎会置位这个信号量。
    semaphore semaphore_imageIsAvailable;

    // 图形队列执行完命令缓冲区后，会置位这个信号量。
    semaphore semaphore_renderingIsOver;

    // 本节先只使用一个主命令缓冲区。
    commandBuffer commandBuffer;

    // 命令池从图形队列族分配命令缓冲区，并允许逐帧重录。
    commandPool commandPool(
        graphicsBase::Base().QueueFamilyIndex_Graphics(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // 真正向命令池申请一个命令缓冲区对象。
    commandPool.AllocateBuffers(commandBuffer);

    // 到这里为止，渲染循环需要的最小同步与命令对象已经备齐。
    while (!glfwWindowShouldClose(pWindow))
    {
        // 如果窗口被最小化，就先阻塞等待窗口恢复，避免无意义地继续渲染。
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
            glfwWaitEvents();

        // 等上一帧的 GPU 工作执行完，并顺手把栅栏复位成未完成状态。
        fence.WaitAndReset();

        // 从交换链中取出当前这一帧要渲染的图像。
        graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);

        // 开始录制这一帧的命令缓冲区。
        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // 这一课先只把渲染循环跑通，真正的绘制命令留到后续章节补充。

        // 结束命令录制，准备提交给图形队列。
        commandBuffer.End();

        // 提交命令时等待“图像可用”信号量，完成后发出“渲染结束”信号量并置位栅栏。
        graphicsBase::Base().SubmitCommandBuffer_Graphics(
            commandBuffer,
            semaphore_imageIsAvailable,
            semaphore_renderingIsOver,
            fence);

        // 呈现阶段等待“渲染结束”信号量，确保不会显示尚未完成的图像。
        graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        // 处理窗口事件。
        glfwPollEvents();

        // 刷新标题栏中的 FPS 显示。
        TitleFps();
    }

    // 退出前统一释放窗口与 Vulkan 资源。
    TerminateWindow();
    return 0;
}
