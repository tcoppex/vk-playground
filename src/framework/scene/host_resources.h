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
 public:
  static bool constexpr kRestructureAttribs{true};
  static bool constexpr kUseAsyncLoad{true};

 public:
  HostResources() = default;

  ~HostResources() = default;

  void setup(); //

  bool load_file(std::string_view filename);

  MaterialProxy const& material(MaterialRef const& ref) const {
    return material_proxies[ref.proxy_index];
  }

  /* --- Host Data --- */

  std::vector<Sampler> samplers{};
  std::vector<ImageData> host_images{};
  std::vector<Texture> textures{};

  std::vector<MaterialProxy> material_proxies{};
  ResourceBuffer<scene::MaterialRef> material_refs{};

  ResourceBuffer<Mesh> meshes{}; //
  std::vector<mat4f> transforms{};

  ResourceBuffer<Skeleton> skeletons{};
  ResourceMap<AnimationClip> animations_map{};

  uint32_t vertex_buffer_size{0u};
  uint32_t index_buffer_size{0u};
  uint32_t total_image_size{0u};

 protected:
  void reset_internal_descriptors();

  MaterialProxy::TextureBinding default_texture_binding_{};
};

} // namespace scene

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_SCENE_HOST_RESOURCES_H
