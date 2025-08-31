#ifndef VKFRAMEWORK_SCENE_MATERIAL_H
#define VKFRAMEWORK_SCENE_MATERIAL_H

#include "framework/common.h"

namespace scene {

/* -------------------------------------------------------------------------- */

struct MaterialStates {
  bool operator==(MaterialStates const& other) const noexcept {
    return alpha_mode == other.alpha_mode;
  }

  bool operator<(MaterialStates const& other) const noexcept {
    if (*this != other) {
      return alpha_mode < other.alpha_mode;
    }
    return false;
  }

  struct Hasher {
    size_t operator()(MaterialStates const& s) const noexcept {
      return std::hash<int>()(static_cast<int>(s.alpha_mode));
    }
  };

  enum class AlphaMode {
    Opaque,
    Mask,
    Blend,
    kCount
  } alpha_mode{AlphaMode::Opaque};
};

// ----------------------------------------------------------------------------

struct MaterialRef {
  // Global index in the Resources buffer.
  // uint32_t index{ kInvalidIndexU32 };

  // TypeIndex of the internal Fx material type.
  std::type_index material_type_index{ kInvalidTypeIndex }; //

  // Index in the Fx intrnal buffer.
  uint32_t material_index{ kInvalidIndexU32 };

  // Static pipeline states.
  MaterialStates states{};
};

// ----------------------------------------------------------------------------

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

#endif // VKFRAMEWORK_SCENE_MATERIAL_H
