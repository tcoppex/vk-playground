#ifndef HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
#define HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H

#include "framework/common.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct MaterialRef {
  uint32_t index{ kInvalidIndexU32 };
  std::type_index material_type_index{ kInvalidTypeIndex }; //
  uint32_t material_index{ kInvalidIndexU32 };
};

// (tmp) Default texture binding when none availables.
struct DefaultTextureBinding {
  uint32_t basecolor;
  uint32_t normal;
  uint32_t roughness_metallic;
  uint32_t occlusion;
  uint32_t emissive;
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
