// File      : codec_api_test.cpp
// Brief     : Vulkan 编解码 C API 自测（往返、通道保真、探测、预算、指标）
// Author    : 孔昊旻(Haomin Kong)
// Email     : <a645162@qq.com> / <konghm@beim.com.cn>
// Created   : 2026-10-31
//
// 退出码：0 = PASS，2 = SKIP（没有可用 Vulkan 设备），1 = FAIL。
//
// 这个测试刻意只用公开 C API：路由层（ImageCodecOffload）看到的正是这一层，
// 内部 C++ 入口再对也只是「自己测自己」。指标部分与父项目锁死的 golden 常量
// 对拍——换后端不换数字是三个后端共同的契约。
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanImageCodec/image_codec_vulkan.h"

namespace {

int g_failures = 0;

void check(bool ok, const std::string &what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

void checkNear(double got, double want, double tolerance,
               const std::string &what) {
  if (!(std::fabs(got - want) <= tolerance)) {
    std::printf("FAIL: %s (got %.15f, want %.15f, tol %g)\n", what.c_str(), got,
                want, tolerance);
    ++g_failures;
  }
}

// 构造一张 C 侧图像（top-down，stride == width * channels）。
image_codec_vulkan_image makeImage(uint32_t width, uint32_t height,
                                   uint32_t channels, unsigned seed) {
  image_codec_vulkan_image img{};
  img.width = width;
  img.height = height;
  img.channels = channels;
  img.stride = static_cast<size_t>(width) * channels;
  img.data_size = img.stride * height;
  img.data = static_cast<uint8_t *>(std::malloc(img.data_size));
  std::mt19937 rng(seed);
  for (size_t i = 0; i < img.data_size; ++i) {
    img.data[i] = static_cast<uint8_t>(rng() & 0xFF);
  }
  return img;
}

// 与父项目 tests/ico_test.cpp 的 makeImage()/perturb() 逐行相同（同 seed、
// 同 mt19937 用法）：golden 常量就是为这一张图锁定的。
image_codec_vulkan_image makePattern() {
  image_codec_vulkan_image img{};
  img.width = 400;
  img.height = 300;
  img.channels = 3;
  img.stride = static_cast<size_t>(img.width) * img.channels;
  img.data_size = img.stride * img.height;
  img.data = static_cast<uint8_t *>(std::malloc(img.data_size));

  std::mt19937 rng(12345);
  for (uint32_t y = 0; y < img.height; ++y) {
    for (uint32_t x = 0; x < img.width; ++x) {
      for (uint32_t ch = 0; ch < 3; ++ch) {
        const size_t idx = (static_cast<size_t>(y) * img.width + x) * 3 + ch;
        const int v =
            (static_cast<int>(x) * 3 + static_cast<int>(y) * 5 +
             static_cast<int>(ch) * 37 + static_cast<int>(rng() % 7)) &
            0xFF;
        img.data[idx] = static_cast<uint8_t>(v);
      }
    }
  }
  return img;
}

image_codec_vulkan_image makeDisturbed(image_codec_vulkan_image &src) {
  image_codec_vulkan_image out = src;
  out.data = static_cast<uint8_t *>(std::malloc(src.data_size));
  std::memcpy(out.data, src.data, src.data_size);

  std::mt19937 rng(99);
  for (size_t i = 0; i < out.data_size; ++i) {
    const int d = static_cast<int>(rng() % 5) - 2;
    const int nv = static_cast<int>(out.data[i]) + d;
    out.data[i] = static_cast<uint8_t>(nv < 0 ? 0 : (nv > 255 ? 255 : nv));
  }
  return out;
}

// 一次完整往返：编码 -> 解码 -> 逐像素比对。
void roundtrip(image_codec_vulkan_codec *codec, int format, const char *name,
               uint32_t width, uint32_t height, uint32_t channels) {
  image_codec_vulkan_image src =
      makeImage(width, height, channels, 7u + channels);

  image_codec_vulkan_encode_params params{};
  params.tiff_compression = 5;
  params.png_compression = 6;

  size_t needed = 0;
  const image_codec_vulkan_status probe = image_codec_vulkan_encode(
      codec, format, &src, &params, nullptr, &needed, 0);
  check(probe == IMAGE_CODEC_VULKAN_ERR_OUT_OF_MEMORY,
        std::string(name) + ": size query reports OOM-with-size");
  check(needed > 0, std::string(name) + ": size query returns >0");

  std::vector<uint8_t> buffer(needed);
  size_t written = 0;
  const image_codec_vulkan_status st = image_codec_vulkan_encode(
      codec, format, &src, &params, buffer.data(), &written, buffer.size());
  if (st != IMAGE_CODEC_VULKAN_OK) {
    check(false, std::string(name) + ": encode failed (status " +
                     std::to_string(static_cast<int>(st)) + ")");
    image_codec_vulkan_image_free(&src);
    return;
  }
  check(written == needed, std::string(name) + ": two-phase sizes agree");

  image_codec_vulkan_image decoded{};
  const image_codec_vulkan_status ds = image_codec_vulkan_decode(
      codec, format, buffer.data(), written, &decoded);
  if (ds != IMAGE_CODEC_VULKAN_OK) {
    check(false, std::string(name) + ": decode failed (status " +
                     std::to_string(static_cast<int>(ds)) + ")");
    image_codec_vulkan_image_free(&src);
    return;
  }

  const std::string tag = std::string(name) + " ch=" + std::to_string(channels);
  check(decoded.width == src.width && decoded.height == src.height,
        tag + ": geometry");
  // 通道保真：1..4 原样回来，绝不升维或丢 Alpha。
  check(decoded.channels == src.channels, tag + ": channel fidelity");
  check(decoded.stride == src.stride, tag + ": stride");
  check(decoded.data_size == src.data_size, tag + ": data size");
  check(std::memcmp(decoded.data, src.data, src.data_size) == 0,
        tag + ": lossless roundtrip");

  image_codec_vulkan_image_free(&decoded);
  image_codec_vulkan_image_free(&src);
}

} // namespace

int main() {
  // -1 = 自动选（独显优先）；没有设备时明确 SKIP。
  image_codec_vulkan_codec *codec = nullptr;
  const image_codec_vulkan_status cs =
      image_codec_vulkan_codec_create(&codec, -1);
  if (cs == IMAGE_CODEC_VULKAN_ERR_BACKEND_UNAVAILABLE) {
    std::printf("SKIP: no usable Vulkan device\n");
    return 2;
  }
  if (cs != IMAGE_CODEC_VULKAN_OK || codec == nullptr) {
    std::printf("FAIL: codec_create status %d\n", static_cast<int>(cs));
    return 1;
  }

  // ---- 1. 能力表 ----
  image_codec_vulkan_capabilities caps{};
  check(image_codec_vulkan_get_capabilities(codec, &caps) ==
            IMAGE_CODEC_VULKAN_OK,
        "get_capabilities");
  std::printf("device: %s (api %u.%u, %zu MiB)\n", caps.device_name,
              VK_API_VERSION_MAJOR(caps.api_version),
              VK_API_VERSION_MINOR(caps.api_version), caps.total_memory >> 20);
  check(caps.device_name[0] != '\0', "capabilities: device name");
  check(caps.tiff_decode == 1 && caps.tiff_encode == 1,
        "capabilities: tiff both directions");
  check(caps.png_decode == 1 && caps.png_encode == 1,
        "capabilities: png both directions");
  // JPEG 尚未实现时必须如实报 0，好让 router 直接回退而不是先失败一次。
  check(caps.jpeg_decode == 0 && caps.jpeg_encode == 0,
        "capabilities: jpeg reported unsupported");
  check(caps.png_filter_gpu == 1, "capabilities: png filter on GPU");
  check(caps.tiff_lzw_gpu == 0, "capabilities: tiff lzw on CPU (honest)");
  check(caps.note[0] != '\0', "capabilities: note explains the gaps");

  // ---- 2. 格式探测 ----
  {
    int fmt = 12345;
    const uint8_t png_magic[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    check(image_codec_vulkan_probe(png_magic, sizeof(png_magic), &fmt) ==
                  IMAGE_CODEC_VULKAN_OK &&
              fmt == IMAGE_CODEC_VULKAN_FORMAT_PNG,
          "probe: png signature");

    const uint8_t tiff_magic[4] = {'I', 'I', 42, 0};
    check(image_codec_vulkan_probe(tiff_magic, sizeof(tiff_magic), &fmt) ==
                  IMAGE_CODEC_VULKAN_OK &&
              fmt == IMAGE_CODEC_VULKAN_FORMAT_TIFF,
          "probe: tiff signature (little endian)");

    const uint8_t tiff_be[4] = {'M', 'M', 0, 42};
    check(image_codec_vulkan_probe(tiff_be, sizeof(tiff_be), &fmt) ==
                  IMAGE_CODEC_VULKAN_OK &&
              fmt == IMAGE_CODEC_VULKAN_FORMAT_TIFF,
          "probe: tiff signature (big endian)");

    const uint8_t junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    check(image_codec_vulkan_probe(junk, sizeof(junk), &fmt) ==
                  IMAGE_CODEC_VULKAN_OK &&
              fmt == IMAGE_CODEC_VULKAN_FORMAT_UNKNOWN,
          "probe: junk is unknown");
  }

  // ---- 3. TIFF / PNG 往返（1..4 通道）----
  for (uint32_t channels = 1; channels <= 4; ++channels) {
    roundtrip(codec, IMAGE_CODEC_VULKAN_FORMAT_TIFF, "tiff", 53, 37, channels);
    roundtrip(codec, IMAGE_CODEC_VULKAN_FORMAT_PNG, "png", 53, 37, channels);
  }

  // ---- 4. JPEG 必须显式拒绝（不是悄悄返回别的格式）----
  {
    image_codec_vulkan_image img = makeImage(32, 16, 3, 1u);
    size_t needed = 0;
    check(image_codec_vulkan_encode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, &img,
                                    nullptr, nullptr, &needed, 0) ==
              IMAGE_CODEC_VULKAN_ERR_UNSUPPORTED_FORMAT,
          "jpeg encode rejected as unsupported");
    const uint8_t dummy[4] = {0, 0, 0, 0};
    image_codec_vulkan_image out{};
    check(image_codec_vulkan_decode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG,
                                    dummy, sizeof(dummy), &out) ==
              IMAGE_CODEC_VULKAN_ERR_UNSUPPORTED_FORMAT,
          "jpeg decode rejected as unsupported");
    image_codec_vulkan_image_free(&img);
  }

  // ---- 5. 显存查询与上限 ----
  {
    image_codec_vulkan_memory_info mem{};
    check(image_codec_vulkan_device_memory(-1, &mem) == IMAGE_CODEC_VULKAN_OK,
          "device_memory");
    check(mem.total_bytes > 0, "device_memory: total > 0");

    check(image_codec_vulkan_set_memory_limit(codec, 64u << 10) ==
              IMAGE_CODEC_VULKAN_OK,
          "set_memory_limit");
    image_codec_vulkan_memory_info info{};
    check(image_codec_vulkan_get_memory_info(codec, &info) ==
                  IMAGE_CODEC_VULKAN_OK &&
              info.budget_bytes == (64u << 10),
          "get_memory_info: budget reflected");

    // 64 KiB 上限下，400x300x3 的指标 band 缓冲必然放不下 -> OutOfMemory，
    // 而不是「悄悄改用 CPU 算完再报成功」。
    image_codec_vulkan_image a = makePattern();
    image_codec_vulkan_image b = makeDisturbed(a);
    double mse = 0.0;
    check(image_codec_vulkan_metric_mse(codec, &a, &b, &mse) ==
              IMAGE_CODEC_VULKAN_ERR_OUT_OF_MEMORY,
          "tiny budget: metric_mse reports out-of-memory");
    image_codec_vulkan_image_free(&a);
    image_codec_vulkan_image_free(&b);

    // 关掉 GPU 后 PNG/TIFF 走 CPU 参考路径，产物仍必须可用；上限在这里
    // 不再有约束力（CPU 用的是宿主内存）。
    check(image_codec_vulkan_set_memory_limit(codec, 0) ==
              IMAGE_CODEC_VULKAN_OK,
          "clear_memory_limit");
    check(image_codec_vulkan_set_use_gpu(codec, 0) == IMAGE_CODEC_VULKAN_OK,
          "set_use_gpu(0)");
    roundtrip(codec, IMAGE_CODEC_VULKAN_FORMAT_TIFF, "tiff-cpu", 31, 29, 3);
    roundtrip(codec, IMAGE_CODEC_VULKAN_FORMAT_PNG, "png-cpu", 31, 29, 3);
    check(image_codec_vulkan_set_use_gpu(codec, 1) == IMAGE_CODEC_VULKAN_OK,
          "set_use_gpu(1)");
  }

  // ---- 6. 指标 vs 父项目 golden 常量 ----
  {
    image_codec_vulkan_image a = makePattern();
    image_codec_vulkan_image b = makeDisturbed(a);

    constexpr double kGoldenMse = 1.989219444444;
    constexpr double kGoldenPsnr = 45.143976650968;
    constexpr double kGoldenSsim = 0.992878374042;
    constexpr double kGoldenMsSsim = 0.999443606477;

    double mse = 0.0;
    check(image_codec_vulkan_metric_mse(codec, &a, &b, &mse) ==
              IMAGE_CODEC_VULKAN_OK,
          "metric_mse");
    checkNear(mse, kGoldenMse, 1e-9, "metric mse vs golden");

    double psnr = 0.0;
    check(image_codec_vulkan_metric_psnr(codec, &a, &b, &psnr) ==
              IMAGE_CODEC_VULKAN_OK,
          "metric_psnr");
    checkNear(psnr, kGoldenPsnr, 1e-9, "metric psnr vs golden");

    image_codec_vulkan_ssim ssim{};
    check(image_codec_vulkan_metric_ssim(codec, &a, &b, &ssim) ==
              IMAGE_CODEC_VULKAN_OK,
          "metric_ssim");
    checkNear(ssim.ssim, kGoldenSsim, 1e-9, "metric ssim vs golden");
    checkNear(ssim.ms_ssim, kGoldenMsSsim, 1e-9, "metric ms-ssim vs golden");
    check(ssim.levels == 5, "metric levels == 5");

    // 尺寸/通道不一致必须报参数非法，绝不做隐式缩放。
    image_codec_vulkan_image small = makeImage(10, 10, 1, 5u);
    double ignored = 0.0;
    check(image_codec_vulkan_metric_mse(codec, &a, &small, &ignored) ==
              IMAGE_CODEC_VULKAN_ERR_INVALID_ARGUMENT,
          "metric rejects mismatched images");

    image_codec_vulkan_image_free(&small);
    image_codec_vulkan_image_free(&a);
    image_codec_vulkan_image_free(&b);
  }

  // ---- 7. 非法输入 ----
  {
    image_codec_vulkan_image bad = makeImage(8, 8, 3, 9u);
    image_codec_vulkan_image out{};
    check(image_codec_vulkan_decode(codec, IMAGE_CODEC_VULKAN_FORMAT_TIFF,
                                    nullptr, 0, &out) ==
              IMAGE_CODEC_VULKAN_ERR_INVALID_ARGUMENT,
          "decode: null input rejected");

    const uint8_t junk[16] = {0};
    check(image_codec_vulkan_decode(codec, IMAGE_CODEC_VULKAN_FORMAT_TIFF, junk,
                                    sizeof(junk), &out) ==
              IMAGE_CODEC_VULKAN_ERR_DECODE_FAILED,
          "decode: garbage tiff reports decode failure");

    bad.channels = 5;
    size_t needed = 0;
    check(image_codec_vulkan_encode(codec, IMAGE_CODEC_VULKAN_FORMAT_PNG, &bad,
                                    nullptr, nullptr, &needed, 0) ==
              IMAGE_CODEC_VULKAN_ERR_INVALID_ARGUMENT,
          "encode: 5 channels rejected");
    image_codec_vulkan_image_free(&bad);
  }

  image_codec_vulkan_codec_destroy(codec);

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("PASS\n");
  return 0;
}
