/* -------------------------------------------------------------------------- */

#include "framework/renderer/fx/skybox.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"
#include "framework/core/camera.h"

#include "framework/renderer/fx/postprocess/compute/compute_fx.h" //
#include "framework/renderer/fx/postprocess/post_fx_pipeline.h"

/* -------------------------------------------------------------------------- */

void Skybox::init(Context const& context, Renderer& renderer) {
  envmap_.init(context, renderer);

  /* Precalculate the BRDF LUT. */
  compute_specular_brdf_lut(context, renderer); //

  /* Internal sampler */
  sampler_LinearClampMipMap_ = renderer.sampler_pool().get({
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .anisotropyEnable = VK_FALSE,
    .maxLod = VK_LOD_CLAMP_NONE,
  });

  /* Create the skybox geometry on the device. */
  {
    Geometry::MakeCube(cube_);

    cube_.initialize_submesh_descriptors({
      { Geometry::AttributeType::Position, shader_interop::skybox::kAttribLocation_Position },
    });

    auto cmd = context.create_transient_command_encoder();

    vertex_buffer_ = cmd.create_buffer_and_upload(
      cube_.get_vertices(),
      VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
    );
    index_buffer_ = cmd.create_buffer_and_upload(
      cube_.get_indices(),
      VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT
    );

    context.finish_transient_command_encoder(cmd);
    cube_.clear_indices_and_vertices();
  }

  /* Descriptor sets */
  {
    descriptor_set_layout_ = renderer.create_descriptor_set_layout({
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_Skybox_Sampler,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
    });

    descriptor_set_ = renderer.create_descriptor_set(descriptor_set_layout_, {
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_Skybox_Sampler,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = sampler_LinearClampMipMap_, //
            .imageView = envmap_.get_image(Envmap::ImageType::Diffuse).view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        }
      }
    });
  }

  pipeline_layout_ = renderer.create_pipeline_layout({
    .setLayouts = { descriptor_set_layout_ },
    .pushConstantRanges = {
      {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT
                    ,
        .size = sizeof(PushConstant_t),
      }
    },
  });

  /* Create the render pipeline */
  {
    auto shaders{context.create_shader_modules(FRAMEWORK_COMPILED_SHADERS_DIR "skybox/", {
      "skybox.vert.glsl",
      "skybox.frag.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      graphics_pipeline_ = renderer.create_graphics_pipeline(pipeline_layout_, {
        .vertex = {
          .module = shaders[0u].module,
          .buffers = cube_.pipeline_vertex_buffer_descriptors(),
        },
        .fragment = {
          .module = shaders[1u].module,
          .targets = {
            {
              .writeMask = VK_COLOR_COMPONENT_R_BIT
                         | VK_COLOR_COMPONENT_G_BIT
                         | VK_COLOR_COMPONENT_B_BIT
                         | VK_COLOR_COMPONENT_A_BIT
                         ,
            }
          },
        },
        .depthStencil = {
          .depthTestEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        },
        .primitive = {
          .topology = cube_.vk_primitive_topology(),
          .cullMode = VK_CULL_MODE_NONE,
        }
      });
    }

    context.release_shader_modules(shaders);
  }
}

// ----------------------------------------------------------------------------

void Skybox::release(Context const& context, Renderer const& renderer) {
  auto allocator = context.allocator();

  allocator.destroy_image(&specular_brdf_lut_);

  renderer.destroy_pipeline(graphics_pipeline_);
  renderer.destroy_pipeline_layout(pipeline_layout_);
  renderer.destroy_descriptor_set_layout(descriptor_set_layout_);

  allocator.destroy_buffer(index_buffer_);
  allocator.destroy_buffer(vertex_buffer_);

  envmap_.release();
}

// ----------------------------------------------------------------------------

bool Skybox::setup(std::string_view hdr_filename) {
  setuped_ = envmap_.setup(hdr_filename);
  return setuped_;
}

// ----------------------------------------------------------------------------

void Skybox::render(RenderPassEncoder & pass, Camera const& camera) const {
  mat4 view{ camera.view() };
  view[3] = vec4(vec3(0.0f), view[3].w);

  PushConstant_t push_constant{};
  push_constant.viewProjectionMatrix = linalg::mul(camera.proj(), view);
  push_constant.hdrIntensity = 1.0;

  pass.bind_pipeline(graphics_pipeline_);
  {
    pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    pass.push_constant(push_constant, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    pass.bind_vertex_buffer(vertex_buffer_);
    pass.bind_index_buffer(index_buffer_, cube_.vk_index_type());
    pass.draw_indexed(cube_.get_index_count());
  }
}

// ----------------------------------------------------------------------------

void Skybox::compute_specular_brdf_lut(Context const& context, Renderer const& renderer) {
  class IntegrateBRDF final : public ComputeFx {
    const uint32_t kNumSamples{ 1024u };

    PushConstant_t push_constant_{
      .numSamples = kNumSamples,
    };

    bool resize(VkExtent2D const dimension) final {
      if (!ComputeFx::resize(dimension)) {
        return false;
      }

      push_constant_.mapResolution = dimension.width;

      images_ = {
        context_ptr_->create_image_2d(
          push_constant_.mapResolution,
          push_constant_.mapResolution,
          VK_FORMAT_R16G16_SFLOAT,
          VK_IMAGE_USAGE_STORAGE_BIT
        )
      };

      return true;
    }

    void setup(VkExtent2D const dimension) final {
      ComputeFx::setup(dimension);

      context_ptr_->update_descriptor_set(descriptor_set_, {
        {
          .binding = kDefaultStorageImageBindingOutput,
          .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .images = { {
            .imageView = images_[0].view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
          } },
        }
      });
    }

    std::string getShaderName() const final {
      return FRAMEWORK_COMPILED_SHADERS_DIR "skybox/integrate_brdf.comp.glsl";
    }

    std::vector<VkPushConstantRange> getPushConstantRanges() const final {
      return {
        {
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .size = sizeof(push_constant_),
        }
      };
    }

    void pushConstant(GenericCommandEncoder const &cmd) final {
      cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    void releaseImagesAndBuffers() final {
      // Left empty to prevent the output image to be destroyed.
    }
  };

  TPostFxPipeline<IntegrateBRDF> brdf_pipeline{};
  brdf_pipeline.init(context, renderer);
  brdf_pipeline.setup({ kBRDFLutResolution, kBRDFLutResolution });

  auto cmd = context.create_transient_command_encoder();
  {
    brdf_pipeline.execute(cmd);
  }
  context.finish_transient_command_encoder(cmd);

  specular_brdf_lut_ = brdf_pipeline.getImageOutput();
  brdf_pipeline.release();

  // [TODO] Calculate mip-maps.
}

/* -------------------------------------------------------------------------- */