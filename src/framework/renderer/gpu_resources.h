#ifndef VKFRAMEWORK_RENDERER_GPU_RESOURCES_H
#define VKFRAMEWORK_RENDERER_GPU_RESOURCES_H

#include "framework/scene/host_resources.h"
#include "framework/renderer/fx/material/material_fx_registry.h"
#include "framework/renderer/raytracing_scene.h"

class Context;
class ResourceAllocator;
class SamplerPool;
class RenderPassEncoder;
class Camera;

/* -------------------------------------------------------------------------- */

struct GPUResources : scene::HostResources {
 public:
  static bool constexpr kReleaseHostDataOnUpload = true;

 public:
  GPUResources(Renderer const& renderer);

  ~GPUResources();

  /* Load a scene assets from disk to Host memory. */
  bool load_file(std::string_view filename);

  /* Bind mesh attributes to pipeline locations. */
  void initialize_submesh_descriptors(scene::Mesh::AttributeLocationMap const& attribute_to_location);

  /* Upload host resources to Device memory. */
  void upload_to_device(bool const bReleaseHostDataOnUpload = kReleaseHostDataOnUpload);

  /* Construct a texture atlas entry for a descriptor set. */
  DescriptorSetWriteEntry descriptor_set_texture_atlas_entry(uint32_t const binding) const;

  /* Update relevant resources before rendering (eg. shared uniform buffers). */
  void update(Camera const& camera, VkExtent2D const& surfaceSize, float elapsedTime);

  /* Render the scene batch per MaterialFx. */
  void render(RenderPassEncoder const& pass);

  // -------------------------------
  RayTracingSceneInterface const* ray_tracing_scene() const {
    return rt_scene_.get();
  }
  // -------------------------------

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

  // -------------------------------
  std::unique_ptr<RayTracingSceneInterface> rt_scene_{};
  // -------------------------------

  using SubMeshBuffer = std::vector<scene::Mesh::SubMesh const*>;
  using FxHashPair = std::pair< MaterialFx*, scene::MaterialStates >;
  using FxHashPairToSubmeshesMap = std::map< FxHashPair, SubMeshBuffer >;
  EnumArray<FxHashPairToSubmeshesMap, scene::MaterialStates::AlphaMode> lookups_{};

 private:
  Renderer const* renderer_ptr_{};
  Context const* context_ptr_{};
  ResourceAllocator const* allocator_ptr_{};
};

/* -------------------------------------------------------------------------- */

using GLTFScene = std::shared_ptr<GPUResources>;

#endif // VKFRAMEWORK_RENDERER_GPU_RESOURCES_H
