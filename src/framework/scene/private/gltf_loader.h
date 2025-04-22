#ifndef HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_
#define HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_

#include "framework/common.h"
#include "framework/scene/resources.h"
#include "framework/utils/cgltf_wrapper.h"

/* -------------------------------------------------------------------------- */

namespace internal::gltf_loader {

using PointerToStringMap_t = std::unordered_map<void const*, std::string>;
using PointerToSamplerMap_t = std::unordered_map<void const*, VkSampler>;

/* -------------------------------------------------------------------------- */

void ExtractSamplers(
  cgltf_data const* data,
  SamplerPool& sampler_pool,
  PointerToSamplerMap_t &samplers_lut
);

void ExtractTextures(
  cgltf_data const* data,
  std::string const& basename,
  PointerToStringMap_t& texture_names,
  scene::ResourceMap<scene::Texture>& textures_map
);

void ExtractMaterials(
  cgltf_data const* data,
  std::string const& basename,
  PointerToStringMap_t &texture_names,
  PointerToStringMap_t &material_names,
  scene::ResourceMap<scene::Texture>& textures_map,
  scene::ResourceMap<scene::Material>& materials_map
);

void ExtractMeshes(
  cgltf_data const* data,
  std::string const& basename,
  bool const bRestructureAttribs,
  PointerToStringMap_t const& material_names,
  scene::ResourceMap<scene::Texture>& textures_map,
  scene::ResourceMap<scene::Material>& materials_map,
  scene::ResourceMap<scene::Skeleton>& skeletons_map,
  std::vector<std::shared_ptr<scene::Mesh>>& meshes
);

void ExtractAnimations(
  cgltf_data const* data,
  std::string const& basename,
  scene::ResourceMap<scene::Skeleton>& skeletons_map,
  scene::ResourceMap<scene::AnimationClip>& animations_map
);

} // namespace internal::gltf_loader

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_