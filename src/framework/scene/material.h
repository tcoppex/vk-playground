#ifndef HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
#define HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H

#include "framework/common.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct MaterialRef {
  uint32_t index{ kInvalidIndexU32 };

  std::type_index material_type_index{ kInvalidTypeIndex };
  uint32_t material_index{ kInvalidIndexU32 };
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_MATERIAL_H
