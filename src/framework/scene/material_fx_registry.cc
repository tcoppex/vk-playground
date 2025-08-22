#include "framework/scene/material_fx_registry.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"

#include "framework/fx/_experimental/scene/pbr_metallic_roughness.h" //

/* -------------------------------------------------------------------------- */

namespace scene {

void MaterialFxRegistry::setup(Context const& context, Renderer const& renderer) {
  // ----------------
  //
  // It might be more interesting to put the share MaterialFx instances
  // inside Renderer, which will need to move draw to
  //  Renderer::draw(RenderPassEncoder, camera, GLTFScene)
  //
  // ----------------

  map_ = {
    {
      type_index<fx::scene::PBRMetallicRoughnessFx>(),
      new fx::scene::PBRMetallicRoughnessFx()
    }
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

void MaterialFxRegistry::update_texture_atlas(std::function<DescriptorSetWriteEntry(uint32_t)> update_fn) {
  LOG_CHECK( !map_.empty() );

  for (auto [_, fx] : map_) {
    fx->updateDescriptorSetTextureAtlasEntry(
      update_fn( fx->getDescriptorSetTextureAtlasBinding() )
    );
  }
}

// ----------------------------------------------------------------------------

MaterialFx* MaterialFxRegistry::material_fx(MaterialRef const& ref) const {
  if (auto it = map_.find(ref.material_type_index); it != map_.end()) {
    return it->second;
  }
  return nullptr;
}

} // namespace scene

/* -------------------------------------------------------------------------- */

