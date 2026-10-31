# VulkanImageCodec.CodecApi —— 平台无关的编解码 C API。
#
# 与 Vulkan.Image.Codec.Api（Windows 专用的 GL 互操作）区分：那个负责把
# compute 结果零拷贝贴进 GL，这个负责对外暴露 TIFF/PNG 的编解码与指标入口。
# 两者名字接近、职责无关，故独立成库而不是塞进 Api。
add_library(VulkanImageCodec.CodecApi STATIC
  src/image_codec_vulkan.cpp
  "${PROJECT_SOURCE_DIR}/include/VulkanImageCodec/image_codec_vulkan.h")

target_include_directories(VulkanImageCodec.CodecApi PUBLIC
  "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(VulkanImageCodec.CodecApi PUBLIC
  VulkanImageCodec::Common
  VulkanImageCodec::Tiff
  VulkanImageCodec::Png
  VulkanImageCodec::Metrics
  Vulkan::Vulkan)

# 共享库场景下 IMAGE_CODEC_VULKAN_API 走 dllexport 分支；静态库下为空。
target_compile_definitions(VulkanImageCodec.CodecApi PRIVATE
  IMAGE_CODEC_VULKAN_BUILDING_LIB)

add_library(VulkanImageCodec::CodecApi ALIAS VulkanImageCodec.CodecApi)
