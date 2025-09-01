#ifndef VKFRAMEWORK_SCENE_RESOURCES_H
#define VKFRAMEWORK_SCENE_RESOURCES_H

#include "framework/scene/host_resources.h"

#include "framework/renderer/fx/material/material_fx_registry.h"

class Context;
class ResourceAllocator;
class SamplerPool;
class RenderPassEncoder;
class Camera;

/* -------------------------------------------------------------------------- */

namespace scene {

///
/// Holds Host and Device resources for rendering a (glTF) scene.
///
struct Resources : HostResources {
 public:
  static bool constexpr kReleaseHostDataOnUpload = true;

 public:
  Resources(Renderer const& renderer);

  ~Resources();

  /* Load a scene assets from disk to Host memory. */
  bool load_file(std::string_view filename);

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

 private:
  void upload_images(Context const& context);
  void upload_buffers(Context const& context);

 public:
  /* --- Device Data --- */

  std::vector<backend::Image> device_images{};
  backend::Buffer vertex_buffer{};
  backend::Buffer index_buffer{};

 protected:
  backend::Buffer frame_ubo_{};
  backend::Buffer transforms_ssbo_{};

 protected:
  std::unique_ptr<MaterialFxRegistry> material_fx_registry_{};

  // ResourceBuffer<MaterialRef> material_refs_{}; //

  using SubMeshBuffer = std::vector<Mesh::SubMesh const*>;
  using FxHashPair = std::pair< MaterialFx*, MaterialStates >;
  using FxHashPairToSubmeshesMap = std::map< FxHashPair, SubMeshBuffer >;
  EnumArray<FxHashPairToSubmeshesMap, MaterialStates::AlphaMode> lookups_{};

 private:
  Renderer const* renderer_ptr_{};
  Context const* context_ptr_{};
  ResourceAllocator const* allocator_ptr_{};
};

} // namespace scene

/* -------------------------------------------------------------------------- */

using GLTFScene = std::shared_ptr<scene::Resources>;

#endif // VKFRAMEWORK_SCENE_RESOURCES_H
