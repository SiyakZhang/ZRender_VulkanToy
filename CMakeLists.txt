cmake_minimum_required(VERSION 3.24)

# 让 MSVC_RUNTIME_LIBRARY 之类的新式运行库选项按现代规则生效。
cmake_policy(SET CMP0091 NEW)

# 教学工程仍然沿用 ZRender 这个目标名，方便和 VS 工程对照着看。
project(ZRender LANGUAGES CXX)

# 这套代码当前明确面向 Windows。
if(NOT WIN32)
    message(FATAL_ERROR "ZRender 当前课程工程只支持 Windows。")
endif()

# 当前教学环境用到的 GLFW / shaderc 预编译库都是 MSVC 版本，因此这里明确限制工具链。
if(NOT MSVC)
    message(FATAL_ERROR "当前 CMake 工程要求使用 MSVC 工具链。请在 CLion 或 Visual Studio 中选择 Visual Studio / MSVC。")
endif()

# 教程里已经大量使用了 C++20，例如 std::format。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 先从环境变量里拿 Vulkan SDK 根目录。
set(_default_vulkan_sdk_root "")
if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
    set(_default_vulkan_sdk_root "$ENV{VULKAN_SDK}")
endif()

# 这几个变量做成缓存项，CLion 和 VS 的 CMake 设置页里都能直接覆盖。
set(VULKAN_SDK_ROOT "${_default_vulkan_sdk_root}" CACHE PATH "Vulkan SDK 根目录，主要用于补充定位 shaderc。")
set(GLFW_ROOT "D:/Programs/glfw-3.4.bin.WIN64" CACHE PATH "GLFW 预编译包根目录，默认值对齐当前教学环境。")
set(GLM_ROOT "D:/Programs/glm" CACHE PATH "GLM 根目录，默认值对齐当前教学环境。")

# Vulkan 本体优先交给 CMake 官方模块去找。
find_package(Vulkan REQUIRED)

# 如果用户没有显式给 VULKAN_SDK_ROOT，就尝试根据 Vulkan 头文件位置反推一次。
if(VULKAN_SDK_ROOT STREQUAL "")
    set(_first_vulkan_include_dir "")
    if(DEFINED Vulkan_INCLUDE_DIRS AND Vulkan_INCLUDE_DIRS)
        list(GET Vulkan_INCLUDE_DIRS 0 _first_vulkan_include_dir)
    elseif(DEFINED Vulkan_INCLUDE_DIR AND NOT Vulkan_INCLUDE_DIR STREQUAL "")
        set(_first_vulkan_include_dir "${Vulkan_INCLUDE_DIR}")
    endif()

    # CMake 通常会把头文件定位到 .../Include，所以再往上退一级就是 SDK 根目录。
    if(NOT _first_vulkan_include_dir STREQUAL "")
        get_filename_component(_vulkan_include_parent "${_first_vulkan_include_dir}" DIRECTORY)
        get_filename_component(VULKAN_SDK_ROOT "${_vulkan_include_parent}" DIRECTORY)
    endif()
endif()

# shaderc 也是本工程要直接用到的，因此这里必须最终能确定 SDK 根目录。
if(VULKAN_SDK_ROOT STREQUAL "" OR NOT EXISTS "${VULKAN_SDK_ROOT}")
    message(FATAL_ERROR "未能确定 VULKAN_SDK_ROOT，请设置环境变量 VULKAN_SDK，或在 CMake 里传入 -DVULKAN_SDK_ROOT=...")
endif()

# GLFW 头文件在 include/GLFW 下。
find_path(GLFW_INCLUDE_DIR
    NAMES GLFW/glfw3.h
    HINTS "${GLFW_ROOT}"
    PATH_SUFFIXES include
    REQUIRED)

# 这里明确找 glfw3dll 对应的导入库，避免又回到 release 静态库混用 CRT 的老问题。
find_library(GLFW_IMPORT_LIBRARY
    NAMES glfw3dll
    HINTS "${GLFW_ROOT}"
    PATH_SUFFIXES lib-vc2022
    REQUIRED)

# 运行时还需要真正的 glfw3.dll，后面会复制到可执行文件旁边。
find_file(GLFW_RUNTIME_DLL
    NAMES glfw3.dll
    HINTS "${GLFW_ROOT}"
    PATH_SUFFIXES lib-vc2022 bin
    REQUIRED)

# GLM 在这个教程工程里是纯头文件依赖。
find_path(GLM_INCLUDE_DIR
    NAMES glm.hpp gtc/matrix_transform.hpp
    HINTS "${GLM_ROOT}"
    PATH_SUFFIXES "" include
    REQUIRED)

# shaderc 头文件通常跟 Vulkan 头文件一起放在 SDK 的 Include 目录里。
find_path(SHADERC_INCLUDE_DIR
    NAMES shaderc/shaderc.hpp
    HINTS "${VULKAN_SDK_ROOT}"
    PATH_SUFFIXES Include include
    REQUIRED)

# Debug 和 Release 要分别链接不同名字的 combined 库。
find_library(SHADERC_LIBRARY_DEBUG
    NAMES shaderc_combinedd
    HINTS "${VULKAN_SDK_ROOT}"
    PATH_SUFFIXES Lib lib
    REQUIRED)

find_library(SHADERC_LIBRARY_RELEASE
    NAMES shaderc_combined
    HINTS "${VULKAN_SDK_ROOT}"
    PATH_SUFFIXES Lib lib
    REQUIRED)

# 把主要目录先抽出来，后面读起来更直观。
set(ZRENDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ZRender")
set(ZRENDER_SHADER_DIR "${ZRENDER_SOURCE_DIR}/shader")
set(ZRENDER_IMAGE_DIR "${ZRENDER_SOURCE_DIR}/image")

# 这里把常用头文件也挂进目标，IDE 打开时能直接看到教学工程的主要结构。
add_executable(ZRender
    "${ZRENDER_SOURCE_DIR}/main.cpp"
    "${ZRENDER_SOURCE_DIR}/GlfwGeneral.hpp"
    "${ZRENDER_SOURCE_DIR}/GlslCompiler.hpp"
    "${ZRENDER_SOURCE_DIR}/RPFB_Screen.hpp"
    "${ZRENDER_SOURCE_DIR}/VKBase+.h"
    "${ZRENDER_SOURCE_DIR}/VKBase.h"
    "${ZRENDER_SOURCE_DIR}/VKFormat.h"
    "${ZRENDER_SOURCE_DIR}/VKHead.h"
    "${ZRENDER_SOURCE_DIR}/VulkanGraphicsPipelineBuilder.h"
    "${ZRENDER_SOURCE_DIR}/stb_image.h")

# 目标自己的头文件目录放最前面，这样 #include "VKBase.h" 之类能直接工作。
target_include_directories(ZRender PRIVATE
    "${ZRENDER_SOURCE_DIR}"
    "${GLFW_INCLUDE_DIR}"
    "${GLM_INCLUDE_DIR}"
    "${SHADERC_INCLUDE_DIR}")

# Vulkan 用官方导入目标；GLFW 和 shaderc 则按我们上面选定的库精确链接。
target_link_libraries(ZRender PRIVATE
    Vulkan::Vulkan
    "${GLFW_IMPORT_LIBRARY}"
    debug "${SHADERC_LIBRARY_DEBUG}"
    optimized "${SHADERC_LIBRARY_RELEASE}")

# 尽量把 CMake 生成的编译选项对齐到现有 VS 工程。
target_compile_definitions(ZRender PRIVATE UNICODE _UNICODE)

# 教程工程默认使用 /MD 和 /MDd，这样跟现有 vcxproj 保持一致。
set_target_properties(ZRender PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ZRender>")

# 这里补上 UTF-8 和 /permissive-，避免和现有 VS 配置行为差太多。
target_compile_options(ZRender PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->)

# VS 里直接按“启动”时，把这个目标设为默认启动项会更顺手。
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ZRender)

# 运行时依赖的 shader / image 目录都使用相对路径读取，所以每次构建后都复制到输出目录旁边。
add_custom_command(TARGET ZRender POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ZRENDER_SHADER_DIR}" "$<TARGET_FILE_DIR:ZRender>/shader"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ZRENDER_IMAGE_DIR}" "$<TARGET_FILE_DIR:ZRender>/image"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${GLFW_RUNTIME_DLL}" "$<TARGET_FILE_DIR:ZRender>/glfw3.dll"
    COMMENT "复制 GLFW 动态库与教学资源，保证 CLion / VS 直接运行时能找到 shader 和 image")

# 配置阶段顺手把关键路径打出来，出了问题时更容易定位。
message(STATUS "VULKAN_SDK_ROOT = ${VULKAN_SDK_ROOT}")
message(STATUS "GLFW_ROOT = ${GLFW_ROOT}")
message(STATUS "GLM_ROOT = ${GLM_ROOT}")
message(STATUS "GLFW_IMPORT_LIBRARY = ${GLFW_IMPORT_LIBRARY}")
message(STATUS "GLFW_RUNTIME_DLL = ${GLFW_RUNTIME_DLL}")
message(STATUS "SHADERC_LIBRARY_DEBUG = ${SHADERC_LIBRARY_DEBUG}")
message(STATUS "SHADERC_LIBRARY_RELEASE = ${SHADERC_LIBRARY_RELEASE}")
