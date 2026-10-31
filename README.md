# ImageCodecVulkan

Vulkan compute 库：**编解码与画质指标是平台无关的**；只有「Vulkan→OpenGL
零拷贝互操作」那一段是 Windows 专用（WGL + `VK_KHR_external_memory_win32`）。

## 特性
- `VulkanImageCodec::Core`：Vulkan 设备/上下文、可导出 Buffer/Image、compute pipeline（平台无关）。
- `VulkanImageCodec::Common`：编解码共用层——1..4 通道 `ImageBuffer`、host-visible
  缓冲、显存预算记账（`VK_EXT_memory_budget` 查空闲显存）、storage-buffer compute pipeline（平台无关）。
- `VulkanImageCodec::Metrics`：MSE / PSNR / SSIM / MS-SSIM，GPU 累加用 double，
  与 CUDA / OpenCL 后端**逐位一致**（golden 常量锁在父项目 `tests/ico_test.cpp`）。
- `VulkanImageCodec::GlInterop`（Windows）：`wglGetProcAddress` 手写加载 `GL_EXT_memory_object(_win32)`，将 win32 handle 导入 GL buffer/texture。
- `VulkanImageCodec::Api`（Windows）：`GlImagePipeline` 一步完成 compute→GL。
- Windows 上全程 CPU 零搬运：Vulkan `VK_KHR_external_memory_win32` 导出 → GL `glImportMemoryWin32HandleEXT` 导入同一块显存。

## 平台划分（为什么 Linux 也能用）
| 模块 | Windows | Linux / 其它 |
| --- | --- | --- |
| Core / Common / Metrics / Tiff / Png / Jpeg | ✅ | ✅ |
| GlInterop / Api（GL 互操作） | ✅ | 不构建、不导出 |

`VK_USE_PLATFORM_WIN32_KHR` 只在 `_WIN32` 下定义；非 Windows 上没有
`vulkan_win32.h`，任何无条件引用都会让整个仓库连 Vulkan 头都包含不进来。

## 依赖
- vcpkg：`vulkan`（headers + loader）、`zlib`（PNG 的 deflate/inflate）
- GLSL 编译器：构建期需要 `glslc`（Vulkan SDK）或 `glslangValidator`
  （Linux 发行版通常只提供后者，如 `apt install glslang-tools`）。
  两者都只用来生成 SPIR-V，**不提交 `.spv`**，运行期也不需要它们。
- 指标需要设备支持 `shaderFloat64`：double 累加是对拍的前提，申请不到会明确失败，
  不会悄悄退化成 float。
- Windows GL 互操作另需：`opengl32.lib` / `user32.lib` / `gdi32.lib`，驱动支持
  `GL_EXT_memory_object` / `GL_EXT_memory_object_win32`（NVIDIA/AMD 现代桌面驱动均支持）。

## 构建
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVIC_BUILD_TESTS=ON
cmake --build build --config Debug
```

## 测试
```bash
ctest --test-dir build -C Debug --output-on-failure   # 或
python tests/run_test.py
```
退出码约定：0=PASS、2=SKIP（环境不支持扩展）、1=FAIL。

## 结构
```
include/VulkanImageCodec/vic.hpp       # 公共头（唯一入口，含全部对外类定义）
src/libs/Vulkan.Image.Codec.Core/      # 设备/可导出资源/compute（平台无关）
src/libs/Vulkan.Image.Codec.Common/    # ImageBuffer / 缓冲 / 显存预算 / pipeline（平台无关）
src/libs/Vulkan.Image.Codec.Metrics/   # MSE / PSNR / SSIM / MS-SSIM（GPU + CPU 参考）
src/libs/Vulkan.Image.Codec.GlInterop/ # GL 扩展加载 + 导入（仅 Windows）
src/libs/Vulkan.Image.Codec.Api/       # GlImagePipeline 高层封装（仅 Windows）
cmake/spv_to_header.cmake              # .spv -> uint32_t 数组头（各模块共用）
tests/                                 # C++ 测试（codec_metrics_test 平台无关）
```
