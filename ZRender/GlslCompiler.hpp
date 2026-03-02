#pragma once
#include "VKHead.h"
#include <shaderc/shaderc.hpp>

// Release 下优先直接链接 shaderc_combined，省去额外依赖项。
#ifdef NDEBUG
#pragma comment(lib, "shaderc_combined.lib")
#else
// Debug 下优先链接 SDK 自带的 debug 版 combined 库，避免混入 release CRT。
#pragma comment(lib, "shaderc_combinedd.lib")
#endif

namespace vulkan
{
    class fCompileGlslToSpv
    {
        // 先声明这个静态工具函数，供 includer 和文件路径重载共用。
        static void LoadFile(const char* filepath, std::vector<char>& binaries);

        struct includer : public shaderc::CompileOptions::IncluderInterface
        {
            // 这里直接继承 shaderc 要求的结果结构体，并顺手托管路径字符串和文件内容。
            struct result_t : shaderc_include_result
            {
                std::string filepath;
                std::vector<char> code;
            };

            shaderc_include_result* GetInclude(
                const char* requested_source,
                shaderc_include_type,
                const char* requesting_source,
                size_t) override
            {
                // shaderc 要求返回动态分配的结果对象，稍后会回调 ReleaseInclude 释放。
                auto& result = *(new result_t);
                auto& filepath = result.filepath;
                auto& code = result.code;

                // 先拿到“发起 include 的那个文件”的路径。
                if (requesting_source)
                    filepath = requesting_source;

                // 再把末尾的文件名替换成被 include 的相对路径，得到真实待读取路径。
                const size_t separatorPos = filepath.find_last_of("/\\");
                if (separatorPos == std::string::npos)
                    filepath = requested_source;
                else
                    filepath.replace(filepath.begin() + separatorPos + 1, filepath.end(), requested_source);

                // 把被包含文件的源码读进来。
                LoadFile(filepath.c_str(), code);

                // 按 shaderc 规定，把返回结构体的几个指针字段都指向托管好的内存。
                result.source_name = filepath.c_str();
                result.source_name_length = filepath.size();
                result.content = code.data();
                result.content_length = code.size();
                result.user_data = this;
                return &result;
            }

            void ReleaseInclude(shaderc_include_result* data) override
            {
                // 只有转回 result_t*，string 和 vector 的析构器才能正确执行。
                delete static_cast<result_t*>(data);
            }
        };

        // shaderc::Compiler 负责真正调用 glslang / spirv-tools 完成编译。
        shaderc::Compiler compiler;

        // CompileOptions 用来设置优化等级和 include 解析器。
        shaderc::CompileOptions options;

        // 结果对象必须保存在成员里，因为返回的 span 会引用它内部的 SPIR-V 数据。
        shaderc::SpvCompilationResult result;

    public:
        fCompileGlslToSpv()
        {
            // 教程里优先选择“性能优化”。
            options.SetOptimizationLevel(shaderc_optimization_level_performance);

            // 把上面的 includer 接给 shaderc，使 #include "xxx" 能在运行期解析。
            options.SetIncluder(std::make_unique<includer>());
        }

        std::span<const uint32_t> operator()(
            std::span<const char> code,
            const char* filepath,
            const char* entry = "main")
        {
            // shaderc 不负责读文件，这里直接把源码内存和文件路径一并交给它。
            result = compiler.CompileGlslToSpv(
                code.data(),
                code.size(),
                shaderc_glsl_infer_from_source,
                filepath ? filepath : "",
                entry ? entry : "main",
                options);

            // 无论是报错还是警告，统一交给日志流输出。
            if (!result.GetErrorMessage().empty())
                std::cout << result.GetErrorMessage();

            // 编译失败就返回空 span，让调用方自行决定后续如何处理。
            if (result.GetCompilationStatus() != shaderc_compilation_status_success)
                return {};

            // shaderc 迭代器以 uint32_t 为单位产出 SPIR-V 字。
            return {result.begin(), size_t(result.end() - result.begin())};
        }

        std::span<const uint32_t> operator()(const char* filepath, const char* entry = "main")
        {
            // 文件路径重载只负责把源码读进内存，再复用上面的核心编译逻辑。
            std::vector<char> binaries;
            LoadFile(filepath, binaries);

            if (binaries.empty())
                return {};

            return (*this)(std::span<const char>(binaries.data(), binaries.size()), filepath, entry);
        }
    };

    inline void fCompileGlslToSpv::LoadFile(const char* filepath, std::vector<char>& binaries)
    {
        // 每次读取前先清空旧内容，避免上次残留影响本次编译。
        binaries.clear();

        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file)
        {
            std::cout << std::format("[ fCompileGlslToSpv ] ERROR\nFailed to open the file: {}\n", filepath);
            return;
        }

        const std::streampos endPosition = file.tellg();
        if (endPosition < 0)
        {
            std::cout << std::format("[ fCompileGlslToSpv ] ERROR\nFailed to get file size: {}\n", filepath);
            return;
        }

        const size_t fileSize = size_t(endPosition);
        binaries.resize(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(binaries.data()), std::streamsize(fileSize));

        if (!file && fileSize)
        {
            std::cout << std::format("[ fCompileGlslToSpv ] ERROR\nFailed to read the file: {}\n", filepath);
            binaries.clear();
        }
    }
}
