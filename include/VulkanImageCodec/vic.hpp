#pragma once
// VulkanImageCodec —— Vulkan compute 结果零拷贝传输到 OpenGL 的 C++ RAII 库。
// 依赖：Vulkan (vcpkg), GL 仅通过本头不暴露；GL 对象以 uint32_t id 传递。
#include <cstdint>
#include <string>
#include <memory>

// vk_platform.h 不自动定义 VK_USE_PLATFORM_WIN32_KHR，需在首个 vulkan.h 包含前
// 显式声明，vulkan.h 才会引入 vulkan_win32.h（Win32 句柄类型）。所有内部头都以
// 本头为公共祖先，在此定义可保证任何 TU 中先于 vulkan.h 生效。
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif

namespace vic {

enum class Result : int {
  Ok = 0,
  Error = 1,
  UnsupportedExtension = 2,  // 互操作扩展缺失（Vulkan 或 GL）→ 应视为 SKIP
  InitFailed = 3,
};

struct DeviceInfo {
  std::string name;
  uint32_t apiVersion = 0;
  bool supportsExternalMemoryWin32 = false;  // VK_KHR_external_memory_win32 可用
};

// ---- 实现分布：Core / GlInterop / Api（见 src/libs/*）----

class Context;          // Vulkan instance/device/queue（Core）
class ExportableBuffer; // 可导出 VkBuffer（Core）
class ExportableImage;  // 可导出 VkImage（Core）
class Compute;          // compute pipeline + dispatch（Core）
class GlInterop;        // GL 扩展加载 + buffer/texture 导入（GlInterop）
class GlImagePipeline;  // 高层：compute→GL 一步到位（Api）

}  // namespace vic

// ---- 唯一入口（umbrella）：包含本头即得到全部对外类的完整定义 ----
// 解析依赖 VulkanImageCodec::Api 传播的 PUBLIC include 目录：
//   include/  Core/src  GlInterop/src  Api/src
// 各具体头内部对 <VulkanImageCodec/vic.hpp> 的再包含由 #pragma once 保证安全。
#include "context.h"
#include "exportable.h"
#include "compute.h"
#include "gl_interop.h"
#include "gl_image_pipeline.h"
