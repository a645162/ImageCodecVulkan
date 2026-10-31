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

// 与 makePattern() 同一套图案公式，但尺寸/通道数可变。
// 宽高刻意取奇数（61x37），以便把 MCU 补齐、色度上采样在右/下边缘的
// clamp 分支都走到——只用 8 的整数倍尺寸，补丁区的错永远发现不了。
image_codec_vulkan_image makePattern_for(uint32_t width, uint32_t height,
                                         uint32_t channels) {
  image_codec_vulkan_image img{};
  img.width = width;
  img.height = height;
  img.channels = channels;
  img.stride = static_cast<size_t>(width) * channels;
  img.data_size = img.stride * height;
  img.data = static_cast<uint8_t *>(std::malloc(img.data_size));

  std::mt19937 rng(12345);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      for (uint32_t ch = 0; ch < channels; ++ch) {
        const size_t idx = (static_cast<size_t>(y) * width + x) * channels + ch;
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

// JPEG 往返用的尺寸：奇数，专门压 MCU 边界。
uint32_t widthOf(uint32_t channels) { return channels == 1 ? 61u : 65u; }
uint32_t heightOf(uint32_t channels) { return channels == 1 ? 37u : 39u; }

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

// 有损往返：编码 -> 解码 -> 比 PSNR。JPEG 不要求逐字节相等，但要求
// 「通道数不变、几何不变、画质不低于阈值」，否则一个「编码成功但内容全错」
// 的实现也能通过。
void jpegRoundtrip(image_codec_vulkan_codec *codec, uint32_t channels,
                   int subsampling, double min_psnr, const char *name) {
  image_codec_vulkan_image src =
      makePattern_for(widthOf(channels), heightOf(channels), channels);

  image_codec_vulkan_encode_params params{};
  params.jpeg_quality = 90;
  params.jpeg_subsampling = subsampling;

  size_t needed = 0;
  const image_codec_vulkan_status probe =
      image_codec_vulkan_encode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, &src,
                                &params, nullptr, &needed, 0);
  check(probe == IMAGE_CODEC_VULKAN_ERR_OUT_OF_MEMORY,
        std::string(name) + ": size query reports OOM-with-size");
  if (needed == 0) {
    check(false, std::string(name) + ": size query returns >0");
    image_codec_vulkan_image_free(&src);
    return;
  }

  std::vector<uint8_t> buffer(needed);
  size_t written = 0;
  const image_codec_vulkan_status st = image_codec_vulkan_encode(
      codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, &src, &params, buffer.data(),
      &written, buffer.size());
  if (st != IMAGE_CODEC_VULKAN_OK) {
    check(false, std::string(name) + ": encode failed (status " +
                     std::to_string(static_cast<int>(st)) + ")");
    image_codec_vulkan_image_free(&src);
    return;
  }
  if (written >= 3) {
    check(buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF,
          std::string(name) + ": output starts with SOI");
  } else {
    check(false, std::string(name) + ": encoded output too short");
  }

  image_codec_vulkan_image decoded{};
  const image_codec_vulkan_status ds = image_codec_vulkan_decode(
      codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, buffer.data(), written, &decoded);
  if (ds != IMAGE_CODEC_VULKAN_OK) {
    check(false, std::string(name) + ": decode failed (status " +
                     std::to_string(static_cast<int>(ds)) + ")");
    image_codec_vulkan_image_free(&src);
    return;
  }

  check(decoded.width == src.width && decoded.height == src.height,
        std::string(name) + ": geometry");
  // 通道保真：灰度进必须灰度出。
  check(decoded.channels == channels, std::string(name) + ": channel fidelity");

  double psnr = 0.0;
  const image_codec_vulkan_status ms =
      image_codec_vulkan_metric_psnr(codec, &src, &decoded, &psnr);
  check(ms == IMAGE_CODEC_VULKAN_OK, std::string(name) + ": psnr computed");
  if (ms == IMAGE_CODEC_VULKAN_OK) {
    check(psnr >= min_psnr, std::string(name) +
                                ": psnr >= " + std::to_string(min_psnr) +
                                " (got " + std::to_string(psnr) + ")");
    std::printf("  %-16s psnr=%.2f dB\n", name, psnr);
  }

  image_codec_vulkan_image_free(&decoded);
  image_codec_vulkan_image_free(&src);
}

// GPU 与 CPU 参考路径必须逐位一致：这是「着色器写对了」的唯一证据。
// 只比 PSNR 是不够的——一条少加半个 128 的 IDCT 也能到 40 dB，但那份 JPEG
// 已经不是别的后端能读出来的那份了。两边共用同一张基矩阵、同一累加顺序，
// 所以这里要求字节级相等，而不是「差一点点」。
void jpegGpuCpuParity(image_codec_vulkan_codec *codec, uint32_t channels,
                      int subsampling, const char *name) {
  image_codec_vulkan_image src =
      makePattern_for(widthOf(channels), heightOf(channels), channels);

  image_codec_vulkan_encode_params params{};
  params.jpeg_quality = 90;
  params.jpeg_subsampling = subsampling;

  const auto encodeOnce = [&](int use_gpu, std::vector<uint8_t> *out) {
    size_t needed = 0;
    if (image_codec_vulkan_set_use_gpu(codec, use_gpu) !=
        IMAGE_CODEC_VULKAN_OK) {
      return false;
    }
    if (image_codec_vulkan_encode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, &src,
                                  &params, nullptr, &needed,
                                  0) != IMAGE_CODEC_VULKAN_ERR_OUT_OF_MEMORY) {
      return false;
    }
    out->assign(needed, 0);
    size_t written = 0;
    return image_codec_vulkan_encode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG,
                                     &src, &params, out->data(), &written,
                                     out->size()) == IMAGE_CODEC_VULKAN_OK &&
           (out->resize(written), true);
  };

  std::vector<uint8_t> gpu_bytes;
  std::vector<uint8_t> cpu_bytes;
  const bool ok_gpu = encodeOnce(1, &gpu_bytes);
  const bool ok_cpu = encodeOnce(0, &cpu_bytes);
  image_codec_vulkan_set_use_gpu(codec, 1); // 立刻复原，别把后续测试带偏

  if (!ok_gpu || !ok_cpu) {
    check(false, std::string(name) + ": gpu/cpu encode both succeed");
    image_codec_vulkan_image_free(&src);
    return;
  }
  check(gpu_bytes == cpu_bytes,
        std::string(name) + ": GPU and CPU encoders agree byte for byte");

  // 解码侧同理：同一个字节流过 GPU 与过 CPU 必须给出同一批像素。
  const auto decodeOnce = [&](int use_gpu, image_codec_vulkan_image *out) {
    if (image_codec_vulkan_set_use_gpu(codec, use_gpu) !=
        IMAGE_CODEC_VULKAN_OK) {
      return false;
    }
    return image_codec_vulkan_decode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG,
                                     gpu_bytes.data(), gpu_bytes.size(),
                                     out) == IMAGE_CODEC_VULKAN_OK;
  };

  image_codec_vulkan_image gpu_img{};
  image_codec_vulkan_image cpu_img{};
  const bool ok_gpu_dec = decodeOnce(1, &gpu_img);
  const bool ok_cpu_dec = decodeOnce(0, &cpu_img);
  image_codec_vulkan_set_use_gpu(codec, 1);

  if (!ok_gpu_dec || !ok_cpu_dec) {
    check(false, std::string(name) + ": gpu/cpu decode both succeed");
  } else {
    check(gpu_img.width == cpu_img.width && gpu_img.height == cpu_img.height &&
              gpu_img.channels == cpu_img.channels &&
              gpu_img.data_size == cpu_img.data_size,
          std::string(name) + ": gpu/cpu decode geometry");
    check(std::memcmp(gpu_img.data, cpu_img.data, gpu_img.data_size) == 0,
          std::string(name) + ": GPU and CPU decoders agree pixel for pixel");
  }
  image_codec_vulkan_image_free(&gpu_img);
  image_codec_vulkan_image_free(&cpu_img);
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
  check(caps.jpeg_decode == 1 && caps.jpeg_encode == 1,
        "capabilities: jpeg both directions");
  check(caps.png_filter_gpu == 1, "capabilities: png filter on GPU");
  check(caps.tiff_lzw_gpu == 0, "capabilities: tiff lzw on CPU (honest)");
  check(caps.note[0] != '\0', "capabilities: note explains the offload split");

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

    const uint8_t jpeg_magic[3] = {0xFF, 0xD8, 0xFF};
    check(image_codec_vulkan_probe(jpeg_magic, sizeof(jpeg_magic), &fmt) ==
                  IMAGE_CODEC_VULKAN_OK &&
              fmt == IMAGE_CODEC_VULKAN_FORMAT_JPEG,
          "probe: jpeg signature");

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

  // ---- 4. JPEG 往返（有损：看 PSNR，不比字节）----
  //
  // 灰度走单分量 4:4:4，彩色走 4:2:0 与 4:4:4 两条；两者都必须保持通道数，
  // 绝不允许把灰度复制成 RGB。PSNR 用同一套 C API 指标算——顺带验证
  // 「编码器真的在编码」而不是返回原图。
  // 阈值不是「画质达标线」，而是「内容明显没写错」的兜底：本图每个像素都带
  // 独立随机分量，4:2:0 必然把色度抹平（父项目 tests/ico_test.cpp 里同一张图
  // 4:2:0 也只有 23 dB 上下），所以 4:2:0 只能给到 20 dB，4:4:4 与灰度才
  // 有资格要求 30 dB / 38 dB。真要测画质应该换真实照片，而不是抬阈值。
  jpegRoundtrip(codec, 1, 0, 38.0, "jpeg gray");
  jpegRoundtrip(codec, 3, 420, 20.0, "jpeg rgb 420");
  jpegRoundtrip(codec, 3, 444, 30.0, "jpeg rgb 444");

  // GPU 路径必须与本后端的 CPU 参考逐位一致（不是阈值，是相等）。
  jpegGpuCpuParity(codec, 1, 0, "jpeg gray parity");
  jpegGpuCpuParity(codec, 3, 444, "jpeg rgb 444 parity");
  jpegGpuCpuParity(codec, 3, 420, "jpeg rgb 420 parity");

  // 2/4 通道：JPEG 没有 Alpha 样本，必须显式拒绝（交给别的后端），
  // 绝不静默降维。
  for (uint32_t channels : {2u, 4u}) {
    image_codec_vulkan_image img = makeImage(32, 16, channels, 3u);
    size_t needed = 0;
    check(image_codec_vulkan_encode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, &img,
                                    nullptr, nullptr, &needed, 0) ==
              IMAGE_CODEC_VULKAN_ERR_UNSUPPORTED_FORMAT,
          "jpeg encode rejects " + std::to_string(channels) +
              "-channel image as unsupported");
    image_codec_vulkan_image_free(&img);
  }

  // 垃圾字节不是「不支持」，是解码失败——分类错了路由层就会白白换后端。
  {
    const uint8_t junk[16] = {0};
    image_codec_vulkan_image out{};
    check(image_codec_vulkan_decode(codec, IMAGE_CODEC_VULKAN_FORMAT_JPEG, junk,
                                    sizeof(junk), &out) ==
              IMAGE_CODEC_VULKAN_ERR_DECODE_FAILED,
          "jpeg: garbage reports decode failure");
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
