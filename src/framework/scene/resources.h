#ifndef HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
#define HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H

#include "framework/common.h"

#include "framework/scene/animation.h"
#include "framework/scene/texture.h"
#include "framework/scene/image_data.h"
#include "framework/scene/material.h"
#include "framework/scene/mesh.h"
#include "framework/scene/camera.h"

class Context;
class ResourceAllocator;
class SamplerPool;
class RenderPassEncoder;

// ----

class MaterialFx;

namespace fx::scene {
class PBRMetallicRoughnessFx;
}

/* -------------------------------------------------------------------------- */

namespace scene {

// (might prefer using std::unique_ptr<T>, std::observer_ptr or raw instead here)

template<typename T>
using ResourceMap = std::unordered_map<std::string, std::shared_ptr<T>>;

template<typename T>
using ResourceBuffer = std::vector<std::shared_ptr<T>>;

// ----------------------------------------------------------------------------

///
/// [wip]
/// A Resources struct holds all the host and device data for rendering a scene,
/// usually loaded from a gltf file.
///
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

  /* Create material fx used for rendering [might be move to Renderer]. */
  void prepare_material_fx(Context const& context, Renderer const& renderer); //

  void render(RenderPassEncoder const& pass, Camera const& camera);

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

  // Shared MaterialFx instances.
  std::unordered_map<std::type_index, MaterialFx*> material_to_fx_map{}; //
};

}  // namespace scene

/* -------------------------------------------------------------------------- */

using GLTFScene = std::shared_ptr<scene::Resources>;

#endif // HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
