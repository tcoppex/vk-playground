#ifndef HELLO_VK_SKYBOX_H
#define HELLO_VK_SKYBOX_H

#include "framework/common.h"

#include "framework/backend/command_encoder.h"
#include "framework/renderer/pipeline.h"
#include "framework/scene/mesh.h"

namespace shader_interop::skybox {
#include "shaders/skybox_interop.h" //
}


class Context;
class Renderer;
class Camera;

/* -------------------------------------------------------------------------- */

class Skybox {
 public:
  static constexpr uint32_t kDiffuseEnvmapResolution{ 1024u };

  static constexpr uint32_t kIrradianceEnvmapResolution{ 256u };
  static constexpr uint32_t kIrradianceEnvmapSampleCount{ 128u };

  static constexpr uint32_t kSpecularEnvmapResolution{ 256u };
  static constexpr uint32_t kSpecularEnvmapSampleCount{ 64u };
  static const uint32_t kSpecularEnvmapLevelCount{
    static_cast<uint32_t>(std::log2(kSpecularEnvmapResolution))
  };

  static constexpr uint32_t kBRDFLutResolution{ 512u };

 public:
  enum {
    Compute_TransformSpherical,
    Compute_IntegrateBRDF,
    Compute_Irradiance,
    Compute_Specular,


    Compute_IrradianceSHCoeff,
    Compute_ReduceSHCoeff,
    Compute_IrradianceTransfer,


    Compute_kCount,
  };

 public:
  Skybox() = default;

  void init(Context const& context, Renderer const& renderer);

  void release();

  bool setup(Context const& context, Renderer const& renderer, std::string_view hdr_filename); //

  void render(RenderPassEncoder & pass, Camera const& camera);

  backend::Image get_irradiance_envmap() const {
    return irradiance_envmap_;
  }

  backend::Buffer get_irradiance_matrices_buffer() const {
    return irradiance_matrices_buffer_;
  }

 private:
  void compute_irradiance_sh_coeff(Context const& context, Renderer const& renderer);

  void compute_irradiance(Context const& context, Renderer const& renderer);

  void compute_specular(Context const& context, Renderer const& renderer);

  void compute_brdf_lut();

 private:
  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::skybox::PushConstant push_constant_{};

  VkPipelineLayout pipeline_layout_{};

  std::array<Pipeline, Compute_kCount> compute_pipelines_{};
  Pipeline graphics_pipeline_{};

  backend::Image diffuse_envmap_{};
  backend::Image irradiance_envmap_{};
  backend::Image specular_envmap_{};
  backend::Image brdf_lut_{};

  scene::Mesh cube_{};
  backend::Buffer vertex_buffer_{};
  backend::Buffer index_buffer_{};

  // Have internal sampler (eg. ClampToEdge) until renderer sampler map
  // implementation.
  VkSampler sampler_{};

  // ----

  backend::Buffer irradiance_matrices_buffer_{};
};

// ----------------------------------------------------------------------------

class Probe {
 public:
  enum class CubeFace {
    PosX = 0,
    NegX,
    PosY,
    NegY,
    PosZ,
    NegZ,

    kCount
  };

 public:
  static constexpr uint32_t kDefaultResolution{ 1024u };

  // Array to iter over cubeface enums.
  static EnumArray<CubeFace, CubeFace> const kIterFaces;

  // View matrices for each faces of an axis aligned cube.
  static EnumArray<mat4, CubeFace> const kViewMatrices;
};

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_SKYBOX_H