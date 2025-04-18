/* -------------------------------------------------------------------------- */
//
//    09 - post processing
//
//  Where we stay in image space to improve our rendering.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"

#include "framework/scene/camera.h"
#include "framework/scene/arcball_controller.h"

#include "framework/fx/_experimental/fx_pipeline.h"
#include "framework/fx/_experimental/compute/depth_minmax.h"
#include "framework/fx/_experimental/fragment/normaldepth_edge.h"
#include "framework/fx/_experimental/fragment/object_edge.h"

namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

/**
 * Simple render pass for gltf objects, creating two texture outputs :
 *  - RGBA_32F Color Texture,
 *  - RGBA_32F Data texture (XY Normal + Z Depth + W ObjectId).
**/
class SceneFx final : public FragmentFx {
 private:
  static constexpr uint32_t kMaxNumTextures = 128u;

 public:
  void init(Context const& context, Renderer const& renderer) final {
    FragmentFx::init(context, renderer);

    uniform_buffer_ = allocator_->create_buffer(
      sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    );
  }

  void release() final {
    allocator_->destroy_buffer(uniform_buffer_);
    gltf_model_->release(allocator_); //

    FragmentFx::release();
  }

 protected:
  std::string getVertexShaderName() const final {
    return COMPILED_SHADERS_DIR "scene.vert.glsl";
  }

  std::string getShaderName() const final {
    return COMPILED_SHADERS_DIR "scene.frag.glsl";
  }

  void createRenderTarget(VkExtent2D const dimension) final {
    render_target_ = renderer_ptr_->create_render_target({
      .color_formats = {
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
      },
      .depth_stencil_format = VK_FORMAT_D24_UNORM_S8_UINT,
      .size = dimension,
      .sampler = renderer_ptr_->get_default_sampler(),
    });

    // Set the clear values for color attachments.
    render_target_->set_color_clear_value({ 0.67f, 0.82f, 0.69f, 0.0f }, 0u);
    render_target_->set_color_clear_value({ 0.0f, 0.0f, -1.0f, 0.0f }, 1u);
  }

  std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const final {
    return {
      {
        .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
      },
      {
        .binding = shader_interop::kDescriptorSetBinding_Sampler,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kMaxNumTextures,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      }
    };
  }

  std::vector<VkPushConstantRange> getPushConstantRanges() const final {
    return {
      {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(push_constant_),
      }
    };
  }

  void updatePushConstant(GenericCommandEncoder const &cmd) final {
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const final {
    return {
      .dynamicStates = {
        VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
      },
      .vertex = {
        .module = shaders[0u].module,
      },
      .fragment = {
        .module = shaders[1u].module,
        .targets = {
          { .format = render_target_->get_color_attachment(0).format },
          { .format = render_target_->get_color_attachment(1).format }
        },
      },
      .depthStencil = {
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      },
      .primitive = {
        .cullMode = VK_CULL_MODE_BACK_BIT,
      }
    };
  }

  void draw(RenderPassEncoder const& pass) final {
    uint32_t instance_index = 0u;
    for (auto const& mesh : gltf_model_->meshes) {
      pass.set_primitive_topology(mesh->get_vk_primitive_topology());

      push_constant_.model.worldMatrix = linalg::mul(
        world_matrix_,
        mesh->world_matrix
      );

      for (auto const& submesh : mesh->submeshes) {
        if (auto mat = submesh.material; mat && mat->albedoTexture) {
          push_constant_.model.albedo_texture_index = mat->albedoTexture->texture_index;
          push_constant_.model.material_index = mat->index;
          push_constant_.model.instance_index = instance_index++;
        }
        updatePushConstant(pass);
        pass.draw(submesh.draw_descriptor, gltf_model_->vertex_buffer, gltf_model_->index_buffer);
      }
    }
  }

 public:
  void setModel(GLTFScene model) {
    LOG_CHECK(model->textures.size() <= kMaxNumTextures); //

    gltf_model_ = model;

    /* Update the Sampler Atlas descriptor with the currently loaded textures. */
    DescriptorSetWriteEntry texture_atlas_entry{
      .binding = shader_interop::kDescriptorSetBinding_Sampler,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };
    for (auto const& tex : gltf_model_->textures) {
      texture_atlas_entry.images.push_back({
        .sampler = renderer_ptr_->get_default_sampler(), //
        .imageView = tex.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      });
    }

    renderer_ptr_->update_descriptor_set(descriptor_set_, {
      {
        .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .buffers = { { uniform_buffer_.buffer } },
      },
      texture_atlas_entry,
    });
  }

  void setWorldMatrix(mat4 const& world_matrix) {
    world_matrix_ = world_matrix;
  }

  void setViewMatrix(mat4 const& view_matrix) {
    push_constant_.viewMatrix = view_matrix;
  }

  void setProjectionMatrix(mat4 const& projection_matrix) {
    host_data_.scene.projectionMatrix = projection_matrix;
  }

  void updateUniforms() {
    context_ptr_->transfer_host_to_device(
      &host_data_, sizeof(host_data_), uniform_buffer_
    );
  }

 private:
  shader_interop::PushConstant push_constant_{};

  shader_interop::UniformData host_data_{};
  backend::Buffer uniform_buffer_{};

  GLTFScene gltf_model_{};
  mat4 world_matrix_{};
};

// ----------------------------------------------------------------------------

/**
 * Customized FxPipeline which use a SceneFx as entry point, and a custom
 * FragmentFx as final compositions tage.
**/
class ToonFxPipeline final : public TFxPipeline<SceneFx> {
 public:
  class ToonComposition final : public FragmentFx {
    std::string getShaderName() const final {
      return COMPILED_SHADERS_DIR "toon.frag.glsl";
    }
  };

 public:
  std::string name() const final {
    return "ToonFxPipeline";
  }

  void init(Context const& context, Renderer const& renderer) final {
    auto entry_fx = getEntryFx();

    auto depth_minmax = add<fx::compute::DepthMinMax>({
      .images = { {entry_fx, 1u} }
    });

    auto normaldepth_edge = add<fx::frag::NormalDepthEdge>({
      .images = { {entry_fx, 1u} },
      .buffers = { {depth_minmax, 0u} }
    });

    auto object_edge = add<fx::frag::ObjectEdge>({
      .images = { {entry_fx, 1u} },
    });

    auto toon = add<ToonComposition>({
      .images = {
        {entry_fx, 0u},
        {normaldepth_edge, 0u},
        {object_edge, 0u}
      },
    });

    FxPipeline::init(context, renderer);
  }
};


/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    wm_->setTitle("09 - అంచున నివసిస్తున్నారు");

    /* Setup the ArcBall camera. */
    {
      camera_.setPerspective(
        lina::radians(60.0f),
        viewport_size_.width,
        viewport_size_.height,
        0.01f,
        500.0f
      );
      camera_.setController(&arcball_controller_);

      arcball_controller_.setView(lina::kTwoPi/16.0f, lina::kTwoPi/8.0f, false);
      arcball_controller_.setDolly(4.0f, false);
    }

    /* Load a glTF Scene. */
    std::string const gltf_filename{ASSETS_DIR "models/"
      "DamagedHelmet.glb"
      // "simple_kaidou.glb"
    };

    auto gltf_scene = renderer_.load_and_upload(gltf_filename, {
      { Geometry::AttributeType::Position,  shader_interop::kAttribLocation_Position },
      { Geometry::AttributeType::Texcoord,  shader_interop::kAttribLocation_Texcoord },
      { Geometry::AttributeType::Normal,    shader_interop::kAttribLocation_Normal   },
    });

    /* Fx Pipeline. */
    toon_pipeline_.init(context_, renderer_);
    toon_pipeline_.setup(renderer_.get_surface_size());

    if (auto sceneFx = toon_pipeline_.getEntryFx(); sceneFx) {
      sceneFx->setModel(gltf_scene);
      sceneFx->setProjectionMatrix(camera_.proj());
      sceneFx->updateUniforms();
    }

    return true;
  }

  void release() final {
    toon_pipeline_.release();
  }

  void frame() final {
    /* Update. */
    {
      const float dt = get_delta_time();

      camera_.update( dt );

      mat4 const world_matrix{
        lina::rotation_matrix_axis(
          vec3(-0.25f, 1.0f, -0.15f),
          get_frame_time() * 0.0f //
        )
      };

      auto sceneFx = toon_pipeline_.getEntryFx();
      sceneFx->setViewMatrix(camera_.view());
      sceneFx->setWorldMatrix(world_matrix);
    }

    /* Render. */
    auto cmd = renderer_.begin_frame();
    {
      /* Main rendering + Toon post-processing. */
      toon_pipeline_.execute(cmd);

      /* Blit the result to a render target (here the main renderer). */
      cmd.blit(toon_pipeline_, renderer_);

      /* Draw UI on top. */
      cmd.draw_ui(renderer_);
    }
    renderer_.end_frame();
  }

  void setup_ui() final {
    ImGui::Begin("Settings");
    {
      ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
      ImGui::Separator();

      if (ImGui::CollapsingHeader("Post-Processing")) {
        toon_pipeline_.setupUI();
      }
    }
    ImGui::End();
  }

 private:
  Camera camera_{};
  ArcBallController arcball_controller_{};

  ToonFxPipeline toon_pipeline_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
