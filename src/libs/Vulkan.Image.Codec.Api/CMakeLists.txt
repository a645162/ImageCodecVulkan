# File: src/libs/Vulkan.Image.Codec.Api/CMakeLists.txt
# Brief: CMake build configuration.
# Author: Haomin Kong
# Email: a645162@163.com
# Created: 2026-08-12

add_library(VulkanImageCodec.Api STATIC src/gl_image_pipeline.cpp)
target_include_directories(VulkanImageCodec.Api PUBLIC
  "${VIC_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(VulkanImageCodec.Api PUBLIC VulkanImageCodec::Core VulkanImageCodec::GlInterop)
add_library(VulkanImageCodec::Api ALIAS VulkanImageCodec.Api)
