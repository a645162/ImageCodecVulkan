add_executable(gl_interop_test gl_interop_test.cpp)
target_link_libraries(gl_interop_test PRIVATE
  VulkanImageCodec::Api
  VulkanImageCodec::GlInterop
  VulkanImageCodec::Core
  opengl32 user32 gdi32)
add_test(NAME gl_interop_test COMMAND gl_interop_test)
set_tests_properties(gl_interop_test PROPERTIES SKIP_RETURN_CODE 2)
