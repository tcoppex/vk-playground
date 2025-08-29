#include "framework/scene/material_fx_registry.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"

#include "framework/fx/material/pbr_metallic_roughness.h" //
#include "framework/fx/material/unlit.h" //

/* -------------------------------------------------------------------------- */

namespace scene {

void MaterialFxRegistry::init(Context const& context, Renderer const& renderer) {
  fx_map_ = {
    {
      fx::scene::PBRMetallicRoughnessFx::MaterialTypeIndex(),
      new fx::scene::PBRMetallicRoughnessFx()
    },
    {
      fx::scene::UnlitMaterialFx::MaterialTypeIndex(),
      new fx::scene::UnlitMaterialFx()
    },
  };

  for (auto [_, fx] : fx_map_) {
    fx->init(context, renderer);
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::release() {
  for (auto [_, fx] : fx_map_) {
    fx->release();
    delete fx;
  }
  *this = {};
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::register_material_states(
  std::type_index const material_type_index,
  scene::MaterialStates const& states
) {
  LOG_CHECK( fx_map_.contains(material_type_index) );
  states_map_[material_type_index].insert(states);
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::setup() {
  for (auto const& [material_type_index, states_set] : states_map_) {
    MaterialFx *fx = fx_map_.at(material_type_index);
    if (!fx->valid()) {
      fx->setup();
    }
    fx->createPipelines({states_set.cbegin(), states_set.cend()});
  }
  states_map_.clear();

  // Keep a simple buffer of active fx.
  active_fx_.clear();
  for (auto const& [_, fx] : fx_map_) {
    if (fx->valid()) {
      active_fx_.emplace_back(fx);
    }
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::push_material_storage_buffers() const {
  for (auto fx : active_fx_) {
    fx->pushMaterialStorageBuffer();
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::update_texture_atlas(std::function<DescriptorSetWriteEntry(uint32_t)> update_fn) {
  for (auto fx : active_fx_) {
    if (uint binding = fx->getTextureAtlasBinding(); binding != kInvalidIndexU32) {
      fx->updateDescriptorSetTextureAtlasEntry(update_fn(binding));
    }
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::update_frame_ubo(backend::Buffer const& buffer) const {
  for (auto fx : active_fx_) {
    fx->updateDescriptorSetFrameUBO(buffer);
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::update_transforms_ssbo(backend::Buffer const& buffer) const {
  for (auto fx : active_fx_) {
    fx->updateDescriptorSetTransformsSSBO(buffer);
  }
}

// ----------------------------------------------------------------------------

MaterialFx* MaterialFxRegistry::material_fx(MaterialRef const& ref) const {
  if (auto it = fx_map_.find(ref.material_type_index); it != fx_map_.end()) {
    return it->second;
  }
  return nullptr;
}

// ----------------------------------------------------------------------------

// template<typename MaterialFxT>
// requires DerivedFrom<MaterialFxT, MaterialFx>
// MaterialFxT::MaterialType const& MaterialFxRegistry::material(MaterialRef const& ref) const {
//   return material_fx<MaterialFxT>(ref)->material(ref.material_index);
// }

} // namespace scene

/* -------------------------------------------------------------------------- */

