#ifndef VKFRAMEWORK_RENDERER_FX_MATERIAL_MATERIAL_FX_REGISTRY_H_
#define VKFRAMEWORK_RENDERER_FX_MATERIAL_MATERIAL_FX_REGISTRY_H_

#include "framework/core/common.h"
#include "framework/backend/types.h"  // for DescriptorSetWriteEntry
#include "framework/scene/material.h" // for scene::MaterialRef, scene::MaterialProxy

#include <set>

class Context;
class Renderer;
class MaterialFx;

/* -------------------------------------------------------------------------- */

class MaterialFxRegistry {
 public:
  MaterialFxRegistry() = default;

  /* Create the initial material fx LUT. */
  void init(Renderer const& renderer);

  /* Release all allocated resources. */
  void release();

  /* Create internal resources for all used MaterialFx. */
  void setup(
    std::vector<scene::MaterialProxy> const& material_proxies,
    std::vector<std::unique_ptr<scene::MaterialRef>>& material_refs
  );

  /* Update DescriptorSet Entries for all MaterialFx. */
  void update_texture_atlas(std::function<DescriptorSetWriteEntry(uint32_t)> update_fn);
  void update_frame_ubo(backend::Buffer const& buffer) const;
  void update_transforms_ssbo(backend::Buffer const& buffer) const;

  /* Push updated for all MaterialFx. */
  void push_material_storage_buffers() const;

  /* Getters */

  MaterialFx* material_fx(scene::MaterialRef const& ref) const;

 private:
  using MaterialModel = scene::MaterialModel; // std::type_index

  using MaterialFxMap     = std::unordered_map<MaterialModel, MaterialFx*>;
  using MaterialStatesMap = std::unordered_map<MaterialModel, std::set<scene::MaterialStates>>;

 private:
  MaterialFxMap fx_map_{};
  MaterialStatesMap states_map_{};
  std::vector<MaterialFx*> active_fx_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_MATERIAL_MATERIAL_FX_REGISTRY_H_
