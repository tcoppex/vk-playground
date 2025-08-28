#include "framework/scene/material_fx_registry.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"

#include "framework/fx/material/pbr_metallic_roughness.h" //
#include "framework/fx/material/unlit.h" //

/* -------------------------------------------------------------------------- */

namespace scene {

void MaterialFxRegistry::setup(Context const& context, Renderer const& renderer) {
  // [ ideally we should preprocess the scene and only setup the pipeline used ]
  map_ = {
    {
      type_index<fx::scene::PBRMetallicRoughnessFx>(),
      new fx::scene::PBRMetallicRoughnessFx() // use std::unique_ptr ?
    },
    {
      type_index<fx::scene::UnlitMaterialFx>(),
      new fx::scene::UnlitMaterialFx()
    },
  };

  for (auto [_, fx] : map_) {
    fx->init(context, renderer);
    fx->setup();
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::release() {
  for (auto [_, fx] : map_) {
    fx->release();
    delete fx;
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::push_material_storage_buffers() const {
  for (auto [_, fx] : map_) {
    fx->pushMaterialStorageBuffer();
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::update_texture_atlas(std::function<DescriptorSetWriteEntry(uint32_t)> update_fn) {
  for (auto [_, fx] : map_) {
    if (uint binding = fx->getTextureAtlasBinding(); binding != kInvalidIndexU32) {
      fx->updateDescriptorSetTextureAtlasEntry(update_fn(binding));
    }
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::update_frame_ubo(backend::Buffer const& buffer) const {
  for (auto [_, fx] : map_) {
    fx->updateDescriptorSetFrameUBO(buffer);
  }
}

// ----------------------------------------------------------------------------

void MaterialFxRegistry::update_transforms_ssbo(backend::Buffer const& buffer) const {
  for (auto [_, fx] : map_) {
    fx->updateDescriptorSetTransformsSSBO(buffer);
  }
}

// ----------------------------------------------------------------------------

MaterialFx* MaterialFxRegistry::material_fx(MaterialRef const& ref) const {
  if (auto it = map_.find(ref.material_type_index); it != map_.end()) {
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

