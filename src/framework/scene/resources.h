#ifndef VKFRAMEWORK_SCENE_RESOURCES_H
#define VKFRAMEWORK_SCENE_RESOURCES_H

#include "framework/common.h"

#include "framework/scene/animation.h"
#include "framework/scene/texture.h"
#include "framework/scene/image_data.h"
#include "framework/scene/material.h"
#include "framework/scene/mesh.h"
#include "framework/renderer/fx/material/material_fx_registry.h"

class Context;
class ResourceAllocator;
class SamplerPool;
class RenderPassEncoder;
class Camera;

/* -------------------------------------------------------------------------- */

namespace scene {

template<typename T>
using ResourceBuffer = std::vector<std::unique_ptr<T>>;

template<typename T>
using ResourceMap = std::unordered_map<std::string, std::unique_ptr<T>>;

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

struct HostResources {
  HostResources() = default;

  virtual ~HostResources() = default;

  virtual void setup();

  virtual bool load_file(std::string_view filename);

  /* --- Host Data --- */

  std::vector<scene::Sampler> samplers{};
  std::vector<ImageData> host_images{};
  std::vector<Texture> textures{}; //

  ResourceBuffer<MaterialRef> material_refs{}; //

  ResourceBuffer<Mesh> meshes{}; //
  std::vector<mat4f> transforms{};

  ResourceBuffer<Skeleton> skeletons{};
  ResourceMap<AnimationClip> animations_map{};

 protected:
  DefaultTextureBinding default_bindings_{};
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

///
/// Holds Host and Device resources for rendering a (glTF) scene.
///
struct Resources : HostResources {
 public:
  static bool constexpr kRestructureAttribs = true;
  static bool constexpr kReleaseHostDataOnUpload = true;

 public:
  Resources(Renderer const& renderer);

  virtual ~Resources();

  /* Create material fx used for rendering [might be move to Renderer]. */
  void setup() override;

  /* Load a scene assets from disk to Host memory. */
  bool load_file(std::string_view filename) override;

 public:
  /* Bind mesh attributes to pipeline locations. */
  void initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location);

  /* Upload host resources to Device memory. */
  void upload_to_device(bool const bReleaseHostDataOnUpload = kReleaseHostDataOnUpload);

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
  void reset_internal_descriptors();

  void upload_images(Context const& context);

  void upload_buffers(Context const& context);

 public:
  // /* --- Host Data --- */

  // std::vector<ImageData> host_images{};
  // std::vector<Texture> textures{};
  // ResourceBuffer<MaterialRef> material_refs{};
  // ResourceBuffer<Skeleton> skeletons{};
  // ResourceBuffer<Mesh> meshes{};
  // std::vector<mat4f> transforms{};

  // ResourceMap<AnimationClip> animations_map{};

  /* --- Device Data --- */

  std::vector<backend::Image> device_images{};
  backend::Buffer vertex_buffer{};
  backend::Buffer index_buffer{};

  uint32_t vertex_buffer_size{0u};
  uint32_t index_buffer_size{0u};
  uint32_t total_image_size{0u};

 private:
  Renderer const* renderer_ptr_{};
  Context const* context_ptr_{};
  ResourceAllocator const* allocator_ptr_{};

  std::unique_ptr<MaterialFxRegistry> material_fx_registry_{};

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
