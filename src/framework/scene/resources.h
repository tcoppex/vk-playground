#ifndef HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
#define HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H

#include "framework/common.h"

#include "framework/scene/animation.h"
#include "framework/scene/texture.h"
#include "framework/scene/image_data.h"
#include "framework/scene/material.h"
#include "framework/scene/mesh.h"

class Context;
class ResourceAllocator;
class SamplerPool;

/* -------------------------------------------------------------------------- */

namespace scene {

template<typename T>
using ResourceMap = std::unordered_map<std::string, std::shared_ptr<T>>;

template<typename T>
using ResourceBuffer = std::vector<std::shared_ptr<T>>;

struct Resources {
 public:
  static bool constexpr kRestructureAttribs = true;
  static bool constexpr kReleaseHostDataOnUpload = true;

 public:
  Resources() = default;

  void release();

  bool load_from_file(std::string_view const& filename, SamplerPool& sampler_pool, bool bRestructureAttribs = kRestructureAttribs);

  /* Bind mesh attributes to pipeline locations. */
  void initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location);

  void upload_to_device(Context const& context, bool const bReleaseHostDataOnUpload = kReleaseHostDataOnUpload);

  DescriptorSetWriteEntry descriptor_set_texture_atlas_entry(uint32_t const binding) const;

 private:
  void reset_internal_device_resource_info();

  void upload_images(Context const& context);

  void upload_buffers(Context const& context);

 public:
  /* --- Host Data --- */

  ResourceBuffer<ImageData> host_images{};
  ResourceBuffer<Texture> textures{};
  ResourceBuffer<Material> materials{};
  ResourceBuffer<Skeleton> skeletons{};
  ResourceBuffer<Mesh> meshes{};

  ResourceMap<AnimationClip> animations_map{};

  /* --- Device Data --- */

  std::vector<backend::Image> device_images{};
  backend::Buffer vertex_buffer{};
  backend::Buffer index_buffer{};

  uint32_t vertex_buffer_size{0u};
  uint32_t index_buffer_size{0u};
  uint32_t total_image_size{0u};

 private:
  std::shared_ptr<ResourceAllocator> allocator{};
};

}  // namespace scene

/* -------------------------------------------------------------------------- */

using GLTFScene = std::shared_ptr<scene::Resources>;

#endif // HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
