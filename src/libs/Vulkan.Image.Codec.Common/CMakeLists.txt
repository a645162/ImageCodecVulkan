# VulkanImageCodec.Common —— 编解码共用层（平台无关）。
#
# 只管「设备 + 缓冲 + compute pipeline」这些所有格式都要用的东西：
# 容器解析属于各格式模块（Tiff / Png / Jpeg），指标属于 Metrics。
add_library(VulkanImageCodec.Common STATIC
  src/vk_codec_device.cpp
  src/vk_buffer.cpp
  src/vk_pipeline.cpp)

target_include_directories(VulkanImageCodec.Common PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(VulkanImageCodec.Common PUBLIC
  VulkanImageCodec::Core
  Vulkan::Vulkan)

add_library(VulkanImageCodec::Common ALIAS VulkanImageCodec.Common)
