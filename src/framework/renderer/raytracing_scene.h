#ifndef VKFRAMEWORK_RENDERER_RAYTRACING_SCENE_H_
#define VKFRAMEWORK_RENDERER_RAYTRACING_SCENE_H_

#include "framework/backend/context.h"
#include "framework/backend/accel_struct.h"
#include "framework/scene/host_resources.h" // for scene::ResourceBuffer
#include "framework/scene/mesh.h"

/* -------------------------------------------------------------------------- */

class RayTracingSceneInterface {
 public:
  virtual ~RayTracingSceneInterface() = default;

  virtual void init(Context const& ctx) = 0;

  virtual void release() = 0;

  virtual void build(
    scene::ResourceBuffer<scene::Mesh> const& meshes,
    backend::Buffer const& vertex_buffer,
    backend::Buffer const& index_buffer
  ) = 0;

  // TODO : build the DescriptorSetAccelerationStructure

 protected:
  virtual bool build_blas(scene::Mesh::SubMesh const& submesh) = 0;

  virtual void build_tlas() = 0;
};

// ----------------------------------------------------------------------------

///
/// Acceleration Structure for a basic raytracer.
///
class RayTracingScene : public RayTracingSceneInterface {
 public:
  RayTracingScene() = default;

  virtual ~RayTracingScene() {
    release();
  }

  void init(Context const& ctx) final;

  void release() final;

  void build(
    scene::ResourceBuffer<scene::Mesh> const& meshes,
    backend::Buffer const& vertex_buffer,
    backend::Buffer const& index_buffer
  ) final;

 protected:
  bool build_blas(scene::Mesh::SubMesh const& submesh) final;

  void build_tlas() final;

 private:
  void build_acceleration_structure(
    backend::AccelerationStructure* as,
    VkPipelineStageFlags2 dstStageMask,
    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo
  );

 private:
  Context const* context_ptr_{};
  VkDeviceAddress vertex_address_{};
  VkDeviceAddress index_address_{};

  std::vector<backend::BLAS> blas_{}; // one per submesh
  backend::TLAS tlas_{};
  backend::Buffer scratch_buffer_{};
};

/* -------------------------------------------------------------------------- */

#endif