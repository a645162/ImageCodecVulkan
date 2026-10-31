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

# PNG：GPU 行滤波与 CPU 参考逐字节对拍 + 1..4 通道无损往返。
add_executable(png_test png_test.cpp)
target_link_libraries(png_test PRIVATE VulkanImageCodec::Png)
add_test(NAME png_test COMMAND png_test)
set_tests_properties(png_test PROPERTIES SKIP_RETURN_CODE 2)

# TIFF：GPU 预测器与 CPU 参考逐字节对拍 + strip/tile、1..4 通道无损往返。
add_executable(tiff_test tiff_test.cpp)
target_link_libraries(tiff_test PRIVATE VulkanImageCodec::Tiff)
add_test(NAME tiff_test COMMAND tiff_test)
set_tests_properties(tiff_test PROPERTIES SKIP_RETURN_CODE 2)

# 公开 C API：往返、通道保真、探测、预算、指标（只用头文件里的入口，与路由层视角一致）。
add_executable(codec_api_test codec_api_test.cpp)
target_link_libraries(codec_api_test PRIVATE VulkanImageCodec::CodecApi)
add_test(NAME codec_api_test COMMAND codec_api_test)
set_tests_properties(codec_api_test PROPERTIES SKIP_RETURN_CODE 2)
