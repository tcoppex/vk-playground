#ifndef VKPLAYGROUND_FRAMEWORK_SCENE_MATERIAL_FX_REGISTRY_H_
#define VKPLAYGROUND_FRAMEWORK_SCENE_MATERIAL_FX_REGISTRY_H_

#include "framework/common.h"
#include <set>

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

  /* Create the initial material fx LUT. */
  void init(Context const& context, Renderer const& renderer);

  /* Release all allocated resources. */
  void release();

  /* Register a new MaterialStates for a given type. */
  void register_material_states(std::type_index const material_type_index, scene::MaterialStates const& states);

  /* Create internal resources for all used MaterialFx. */
  void setup();

  template<typename MaterialFxT>
  requires DerivedFrom<MaterialFxT, MaterialFx>
  std::tuple<MaterialRef, typename MaterialFxT::MaterialType*>
  create_material(scene::MaterialStates const& states) {
    if (auto it = fx_map_.find(MaterialFxT::MaterialTypeIndex()); it != fx_map_.end()) {
      auto [ref, raw_ptr] = it->second->createMaterial(states);
      return {
        ref,
        static_cast<MaterialFxT::MaterialType*>(raw_ptr)
      };
    }
    return {};
  }

  /* Update DescriptorSet Entries for all MaterialFx. */

  void update_texture_atlas(std::function<DescriptorSetWriteEntry(uint32_t)> update_fn);

  void update_frame_ubo(backend::Buffer const& buffer) const;

  void update_transforms_ssbo(backend::Buffer const& buffer) const;

  /* Push updated for all MaterialFx. */

  void push_material_storage_buffers() const;

  /* Getters */

  MaterialFx* material_fx(MaterialRef const& ref) const;

 private:
  // (use std::unique_ptr?)
  using MaterialFxMap     = std::unordered_map<std::type_index, MaterialFx*>;
  using MaterialStatesMap = std::unordered_map<std::type_index, std::set<MaterialStates>>;

  template<typename MaterialFxT>
  std::type_index type_index() const {
    return MaterialFxT::MaterialTypeIndex();
  }

 private:
  MaterialFxMap fx_map_{};
  MaterialStatesMap states_map_{};
  std::vector<MaterialFx*> active_fx_{};
};

}  // namespace scene

/* -------------------------------------------------------------------------- */

#endif // VKPLAYGROUND_FRAMEWORK_SCENE_MATERIAL_FX_REGISTRY_H_
