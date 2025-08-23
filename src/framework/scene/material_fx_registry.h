#ifndef HELLO_VK_FRAMEWORK_SCENE_MATERIAL_FX_REGISTRY_H_
#define HELLO_VK_FRAMEWORK_SCENE_MATERIAL_FX_REGISTRY_H_

#include "framework/common.h"

#include "framework/scene/material.h"
#include "framework/backend/types.h" // for DescriptorSetWriteEntry

class Context;
class Renderer;
class MaterialFx;

namespace fx::scene {
class PBRMetallicRoughnessFx;
}

/* -------------------------------------------------------------------------- */

namespace scene {

class MaterialFxRegistry {
 public:
  MaterialFxRegistry() = default;

  void setup(Context const& context, Renderer const& renderer);

  void release();

  template<typename MaterialFxT>
  requires DerivedFrom<MaterialFxT, MaterialFx>
  std::tuple<MaterialRef, typename MaterialFxT::MaterialType*>
  create_material() {
    if (auto it = map_.find(type_index<MaterialFxT>()); it != map_.end()) {
      auto [ref, raw_ptr] = it->second->createMaterial();
      return {
        ref,
        static_cast<MaterialFxT::MaterialType*>(raw_ptr)
      };
    }
    return {};
  }

  void push_material_storage_buffers() const;

  void update_texture_atlas(std::function<DescriptorSetWriteEntry(uint32_t)> update_fn);

  MaterialFx* material_fx(MaterialRef const& ref) const;

 private:
  using MaterialFxMap = std::unordered_map<std::type_index, MaterialFx*>;

  template<typename MaterialFxT>
  std::type_index type_index() const {
    return MaterialFxT::MaterialTypeIndex();
  }

  MaterialFxMap map_{};
};

}  // namespace scene

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_FRAMEWORK_SCENE_MATERIAL_FX_REGISTRY_H_
