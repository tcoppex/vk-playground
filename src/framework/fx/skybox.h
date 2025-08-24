#ifndef HELLO_VK_FRAMEWORK_FX_SKYBOX_H_
#define HELLO_VK_FRAMEWORK_FX_SKYBOX_H_

#include "framework/common.h"

#include "framework/backend/command_encoder.h"
#include "framework/renderer/pipeline.h"
#include "framework/scene/mesh.h"
#include "framework/fx/envmap.h"

namespace shader_interop::skybox {
#include "framework/shaders/skybox/interop.h"
}

class Context;
class Renderer;
class Camera;

/* -------------------------------------------------------------------------- */

class Skybox {
 public:
  static constexpr uint32_t kBRDFLutResolution{ 512u };

 public:
  Skybox() = default;

  void init(Context const& context, Renderer const& renderer);

  void release(Context const& context, Renderer const& renderer);

  bool setup(std::string_view hdr_filename); //

  void render(RenderPassEncoder& pass, Camera const& camera) const;

  Envmap const& envmap() const {
    return envmap_;
  }

  backend::Image const& specular_cubemap() const {
    return envmap_.get_image(Envmap::ImageType::Specular);
  }

  backend::Image const& irradiance_cubemap() const {
    return envmap_.get_image(Envmap::ImageType::Irradiance);
  }

  bool is_valid() const {
    return setuped_;
  }

 private:
  void compute_brdf_lut();

 private:
  using PushConstant_t = shader_interop::skybox::PushConstant;

  Envmap envmap_{};

  backend::Image brdf_lut_{}; //
  VkSampler sampler_{}; //

  scene::Mesh cube_{};
  backend::Buffer vertex_buffer_{};
  backend::Buffer index_buffer_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};

  VkPipelineLayout pipeline_layout_{};
  Pipeline graphics_pipeline_{};

  bool setuped_{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_FRAMEWORK_FX_SKYBOX_H_
