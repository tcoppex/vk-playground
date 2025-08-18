#ifndef HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
#define HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H

#include "framework/common.h"
#include "framework/scene/texture.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct Material {
  uint32_t index{};

  virtual ~Material() = default;

  vec4f diffuse_color{vec4f(1.0f)};
  uint32_t diffuse_texture_id{std::numeric_limits<uint32_t>::max()};

  bool hasDiffuseTexture() const {
    return diffuse_texture_id != std::numeric_limits<uint32_t>::max();
  }
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
