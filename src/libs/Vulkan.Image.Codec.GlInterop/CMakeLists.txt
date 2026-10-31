# File: src/libs/Vulkan.Image.Codec.GlInterop/CMakeLists.txt
# Brief: CMake build configuration.
# Author: Haomin Kong
# Email: a645162@163.com
# Created: 2026-08-12

add_library(VulkanImageCodec.GlInterop STATIC src/gl_interop.cpp)
target_include_directories(VulkanImageCodec.GlInterop PUBLIC
  "${VIC_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(VulkanImageCodec.GlInterop PUBLIC
  VulkanImageCodec::Core
  opengl32)
add_library(VulkanImageCodec::GlInterop ALIAS VulkanImageCodec.GlInterop)
