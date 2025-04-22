#ifndef HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
#define HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H

#include "framework/common.h"
#include "framework/scene/image.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct Material {
  std::string name{};
  uint32_t index{};

  vec4f baseColor{vec4f(1.0f)};
  std::shared_ptr<Image> albedoTexture{};
  std::shared_ptr<Image> ormTexture{};
  std::shared_ptr<Image> normalTexture{};

  Material(std::string_view _name)
    : name(_name)
  {}
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
