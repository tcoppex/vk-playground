#ifndef VKFRAMEWORK_SCENE_HOST_RESOURCES_H
#define VKFRAMEWORK_SCENE_HOST_RESOURCES_H

#include "framework/common.h"

#include "framework/scene/animation.h"
#include "framework/scene/texture.h"
#include "framework/scene/image_data.h"
#include "framework/scene/material.h"
#include "framework/scene/mesh.h"

/* -------------------------------------------------------------------------- */

namespace scene {

template<typename T>
using ResourceBuffer = std::vector<std::unique_ptr<T>>;

template<typename T>
using ResourceMap = std::unordered_map<std::string, std::unique_ptr<T>>;

// ----------------------------------------------------------------------------

struct HostResources {
  HostResources() = default;

  virtual ~HostResources() = default;

  virtual void setup();

  virtual bool load_file(std::string_view filename);

  /* --- Host Data --- */

  std::vector<scene::Sampler> samplers{};
  std::vector<ImageData> host_images{};
  std::vector<Texture> textures{};

  ResourceBuffer<MaterialRef> material_refs{}; //

  ResourceBuffer<Mesh> meshes{}; //
  std::vector<mat4f> transforms{};

  ResourceBuffer<Skeleton> skeletons{};
  ResourceMap<AnimationClip> animations_map{};

  uint32_t vertex_buffer_size{0u};
  uint32_t index_buffer_size{0u};
  uint32_t total_image_size{0u};

 protected:
  virtual void reset_internal_descriptors();

  DefaultTextureBinding default_bindings_{};
};

} // namespace scene

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_SCENE_HOST_RESOURCES_H
