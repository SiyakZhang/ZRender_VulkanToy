#include "GlfwGeneral.hpp"

int main() {
    if (!InitializeWindow({ 1280,720 }))
        return -1;//����������ķ���ֵ
    while (!glfwWindowShouldClose(pWindow)) {

        /*��Ⱦ���̣������*/

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}