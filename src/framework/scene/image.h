#ifndef HELLO_VK_FRAMEWORK_SCENE_IMAGE_H
#define HELLO_VK_FRAMEWORK_SCENE_IMAGE_H

#include "stb/stb_image.h"
#include "framework/common.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct Image {
  int width{};
  int height{};
  int channels{};

  std::unique_ptr<uint8_t, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free}; //

  Image() = default;

  void clear() {
    pixels.reset();
  }
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_IMAGE_H
