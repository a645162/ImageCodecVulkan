# Vulkan -> OpenGL 零拷贝互操作测试只在 Windows 上有意义（WGL + external_memory_win32）。
if(WIN32)
  add_executable(gl_interop_test gl_interop_test.cpp)
  target_link_libraries(gl_interop_test PRIVATE
    VulkanImageCodec::Api
    VulkanImageCodec::GlInterop
    VulkanImageCodec::Core
    opengl32 user32 gdi32)
  add_test(NAME gl_interop_test COMMAND gl_interop_test)
  set_tests_properties(gl_interop_test PROPERTIES SKIP_RETURN_CODE 2)
endif()

# 编解码 / 指标测试（平台无关）：GPU 结果必须与 CPU 参考逐项对上。
add_executable(codec_metrics_test codec_metrics_test.cpp)
target_link_libraries(codec_metrics_test PRIVATE
  VulkanImageCodec::Metrics
  VulkanImageCodec::Core)
add_test(NAME codec_metrics_test COMMAND codec_metrics_test)
# 退出码 2 = 本机没有可用 Vulkan 设备 / 没有 fp64：属于环境 SKIP，不是失败。
set_tests_properties(codec_metrics_test PROPERTIES SKIP_RETURN_CODE 2)
