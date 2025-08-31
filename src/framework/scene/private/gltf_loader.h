#ifndef VKFRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_
#define VKFRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_

#include "framework/common.h"

#include "framework/renderer/fx/material/material_fx_registry.h" //
#include "framework/scene/resources.h"
#include "framework/scene/private/cgltf_wrapper.h"


/* -------------------------------------------------------------------------- */

namespace internal::gltf_loader {

using PointerToIndexMap_t = std::unordered_map<void const*, uint32_t>;
using PointerToSamplerMap_t = std::unordered_map<void const*, VkSampler>;

// template<typename T>
// using PointerToResourceMap_t = std::unordered_map<void const*, std::unique_ptr<T>>;

/* -------------------------------------------------------------------------- */

PointerToSamplerMap_t ExtractSamplers(
  cgltf_data const* data,
  SamplerPool const& sampler_pool
);

PointerToIndexMap_t ExtractImages(
  cgltf_data const* data,
  std::vector<scene::ImageData>& images
);

PointerToIndexMap_t ExtractTextures(
  cgltf_data const* data,
  PointerToIndexMap_t const& image_indices,
  PointerToSamplerMap_t const& samplers_lut, //
  std::vector<scene::Texture>& textures
);

void PreprocessMaterials(
  cgltf_data const* data,
  MaterialFxRegistry& material_fx_registry
);

PointerToIndexMap_t ExtractMaterials(
  cgltf_data const* data,
  PointerToIndexMap_t const& textures_indices,
  scene::ResourceBuffer<scene::MaterialRef>& material_refs,
  MaterialFxRegistry& material_fx_registry,
  scene::DefaultTextureBinding const& bindings
);

PointerToIndexMap_t ExtractSkeletons(
  cgltf_data const* data,
  scene::ResourceBuffer<scene::Skeleton>& skeletons
);

void ExtractMeshes(
  cgltf_data const* data,
  PointerToIndexMap_t const& materials_indices,
  scene::ResourceBuffer<scene::MaterialRef> const& material_refs,
  PointerToIndexMap_t const& skeleton_indices,
  scene::ResourceBuffer<scene::Skeleton>const& skeletons,
  scene::ResourceBuffer<scene::Mesh>& meshes,
  std::vector<mat4f>& transforms,
  bool const bRestructureAttribs
);

void ExtractAnimations(
  cgltf_data const* data,
  std::string const& basename,
  scene::ResourceMap<scene::Skeleton>& skeletons_map,
  scene::ResourceMap<scene::AnimationClip>& animations_map
);

} // namespace internal::gltf_loader

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_