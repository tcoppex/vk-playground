#ifndef HELLO_VK_FRAMEWORK_SCENE_IMAGE_DATA_H_
#define HELLO_VK_FRAMEWORK_SCENE_IMAGE_DATA_H_

extern "C" {
#include <stb/stb_image.h>
}

#include "framework/common.h"
#include "framework/utils/utils.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct ImageData {
 public:
  static constexpr int32_t kDefaultNumChannels{ STBI_rgb_alpha }; //

 public:
  ImageData() = default;

  ImageData(stbi_uc const* buffer_data, uint32_t const buffer_size)
    : ImageData()
  {
    load(buffer_data, buffer_size);
  }

  ImageData(uint8_t r, uint8_t g, uint8_t b, uint8_t a, int32_t _width = 1, int32_t _height = 1) {
    width = _width;
    height = _height;
    channels = kDefaultNumChannels;
    auto* data = static_cast<uint8_t*>(malloc(width * height * channels));
    int index(0);
    for (int j=0; j<height; ++j) {
      for (int i=0; i<width; ++i, ++index) {
        data[index+0] = r;
        data[index+1] = g;
        data[index+2] = b;
        data[index+3] = a;
      }
    }
    pixels.reset(data);
  }

  bool load(stbi_uc const* buffer_data, uint32_t const buffer_size) {
    auto pixels_data = stbi_load_from_memory(
      buffer_data,
      static_cast<int32_t>(buffer_size),
      &width,
      &height,
      &channels,
      kDefaultNumChannels
    );
    if (pixels_data) {
      pixels.reset(pixels_data);
    }
    return nullptr != pixels_data;
  }

  void release() {
    pixels.reset();
  }

  std::future<bool> loadAsyncFuture(stbi_uc const* buffer_data, uint32_t const buffer_size) {
    if (retrieveImageInfo(buffer_data, buffer_size)) {
      return utils::RunTaskGeneric<bool>([this, buffer_data, buffer_size] {
        return load(buffer_data, buffer_size);
      });
    }
    return {};
  }

  void loadAsync(stbi_uc const* buffer_data, uint32_t const buffer_size) {
    if (retrieveImageInfo(buffer_data, buffer_size)) {
      async_result_ = utils::RunTaskGeneric<bool>([this, buffer_data, buffer_size] {
        return load(buffer_data, buffer_size);
      });
    }
  }

  bool getLoadAsyncResult() {
    return async_result_.valid() ? async_result_.get() : false;
  }

  uint8_t const* getPixels() const {
    return pixels.get();
  }

  uint8_t const* getPixels() {
    return (pixels || (getLoadAsyncResult() && pixels)) ? pixels.get() : nullptr;
  }

  uint32_t getBytesize() const {
    return static_cast<uint32_t>(kDefaultNumChannels * width * height);
  }

 public:
  int32_t width{};
  int32_t height{};
  int32_t channels{}; //

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //

 private:
  bool retrieveImageInfo(stbi_uc const *buffer_data, int buffer_size) {
    return 0 < stbi_info_from_memory(buffer_data, buffer_size, &width, &height, &channels);
  }

  std::future<bool> async_result_;
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_IMAGE_DATA_H_