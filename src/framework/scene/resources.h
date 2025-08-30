#ifndef VKFRAMEWORK_SCENE_RESOURCES_H
#define VKFRAMEWORK_SCENE_RESOURCES_H

#include "framework/common.h"

#include "framework/scene/animation.h"
#include "framework/scene/texture.h"
#include "framework/scene/image_data.h"
#include "framework/scene/material.h"
#include "framework/scene/mesh.h"
#include "framework/scene/camera.h"
#include "framework/scene/material_fx_registry.h"

class Context;
class ResourceAllocator;
class SamplerPool;
class RenderPassEncoder;

/* -------------------------------------------------------------------------- */

namespace scene {

///
/// DevNote : we might prefer using std::unique_ptr<T>, std::observer_ptr or
///           raw pointer instead of shared_ptr.
///

template<typename T>
using ResourceMap = std::unordered_map<std::string, std::shared_ptr<T>>;

template<typename T>
using ResourceBuffer = std::vector<std::shared_ptr<T>>;

// ----------------------------------------------------------------------------

///
/// Holds Host and Device resources for rendering a (glTF) scene.
///
struct Resources {
 public:
  static bool constexpr kRestructureAttribs = true;
  static bool constexpr kReleaseHostDataOnUpload = true;

 public:
  Resources() = default;

  void release();

  /* Create material fx used for rendering [might be move to Renderer]. */
  void prepare_material_fx(Context const& context, Renderer const& renderer); //

  /* Load a scene assets from disk to Host memory. */
  bool load_from_file(std::string_view const& filename, SamplerPool& sampler_pool, bool bRestructureAttribs = kRestructureAttribs);

  /* Bind mesh attributes to pipeline locations. */
  void initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location);

  /* Upload host resources to Device memory. */
  void upload_to_device(Context const& context, bool const bReleaseHostDataOnUpload = kReleaseHostDataOnUpload);

  /* Construct a texture atlas entry for a descriptor set. */
  DescriptorSetWriteEntry descriptor_set_texture_atlas_entry(uint32_t const binding) const;

  /* Update relevant resources before rendering (eg. shared uniform buffers). */
  void update(Camera const& camera, VkExtent2D const& surfaceSize, float elapsedTime);

  /* Render the scene batch per MaterialFx. */
  void render(RenderPassEncoder const& pass);

  template<typename TMaterialFx>
  requires DerivedFrom<TMaterialFx, MaterialFx>
  TMaterialFx* material_fx(MaterialRef const& ref) {
    MaterialFx *fx = material_fx_registry_->material_fx(ref);
    return static_cast<TMaterialFx*>(fx);
  }

 private:
  void reset_internal_device_resource_info();

  void upload_images(Context const& context);

  void upload_buffers(Context const& context);

 public:
  /* --- Host Data --- */

  ResourceBuffer<ImageData> host_images{};
  ResourceBuffer<Texture> textures{};
  ResourceBuffer<MaterialRef> material_refs{};
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
  Context const* context_ptr_{};
  std::shared_ptr<ResourceAllocator> allocator_{};

  DefaultTextureBinding optionnal_texture_binding_{};
  MaterialFxRegistry *material_fx_registry_{};

  backend::Buffer frame_ubo_{};
  backend::Buffer transforms_ssbo_{};

  ///
  /// Limitations : if a MaterialFx have others states changes than its alphamode
  ///                 some might be discarded...
  ///
  using SubMeshBuffer = std::vector<Mesh::SubMesh const*>;
  using FxToSubmeshesMap = std::map< MaterialFx*, SubMeshBuffer >;
  EnumArray<FxToSubmeshesMap, MaterialStates::AlphaMode> lookups_;
};

}  // namespace scene

/* -------------------------------------------------------------------------- */

using GLTFScene = std::shared_ptr<scene::Resources>;

#endif // VKFRAMEWORK_SCENE_RESOURCES_H
