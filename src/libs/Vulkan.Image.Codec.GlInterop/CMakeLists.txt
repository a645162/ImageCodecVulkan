add_library(VulkanImageCodec.GlInterop STATIC src/gl_interop.cpp)
target_include_directories(VulkanImageCodec.GlInterop PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(VulkanImageCodec.GlInterop PUBLIC
  VulkanImageCodec::Core
  opengl32)
add_library(VulkanImageCodec::GlInterop ALIAS VulkanImageCodec.GlInterop)
