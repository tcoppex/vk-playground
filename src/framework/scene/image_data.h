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

  void loadAsync(stbi_uc const* buffer_data, uint32_t const buffer_size) {
    if (getInfo(buffer_data, buffer_size)) {
      async_result_ = utils::RunTaskGeneric<bool>([this, buffer_data, buffer_size] {
        return load(buffer_data, buffer_size);
      });
    }
  }

  std::future<bool> loadAsyncFuture(stbi_uc const* buffer_data, uint32_t const buffer_size) {
    if (getInfo(buffer_data, buffer_size)) {
      return utils::RunTaskGeneric<bool>([this, buffer_data, buffer_size] {
        return load(buffer_data, buffer_size);
      });
    }
    return {};
  }

  bool getLoadAsyncResult() {
    return async_result_.valid() ? async_result_.get() : false;
  }

  void release() {
    pixels.reset();
  }

  uint8_t const* getPixels() const {
    return pixels.get();
  }

  uint8_t const* getPixels() {
    if (pixels || getLoadAsyncResult()) {
      return pixels.get();
    }
    return nullptr;
  }

 public:
  int32_t width{};
  int32_t height{};
  int32_t channels{}; //

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //

 private:
  bool getInfo(stbi_uc const *buffer_data, int buffer_size) {
    return 0 < stbi_info_from_memory(buffer_data, buffer_size, &width, &height, &channels);
  }

  std::future<bool> async_result_;
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_IMAGE_DATA_H_