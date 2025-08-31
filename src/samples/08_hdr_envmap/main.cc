/* -------------------------------------------------------------------------- */
//
//    08 - image based lighting
//
//  Where we use a HDR envmap to illuminate a 3D model.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"

#include "framework/core/camera.h"
#include "framework/core/arcball_controller.h"
#include "framework/renderer/fx/material/impl/pbr_metallic_roughness.h"

namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

static constexpr uint32_t kMaxNumTextures = 128u;

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  using HostData_t = shader_interop::UniformData;

 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    wm_->setTitle("08 - Es werde Licht");

    renderer_.set_color_clear_value({{ 0.55f, 0.65f, 0.75f, 1.0f }});

    /* Setup the ArcBall camera. */
    {
      camera_.setPerspective(
        lina::radians(60.0f),
        viewport_size_.width,
        viewport_size_.height,
        0.05f,
        750.0f
      );
      camera_.setController(&arcball_controller_);

      arcball_controller_.setView(lina::kTwoPi/16.0f, lina::kTwoPi/8.0f, false);
      arcball_controller_.setDolly(4.0f, false);

      host_data_.scene = {
        .projectionMatrix = camera_.proj(),
      };

      /* Directly create and upload the uniform values. */
      uniform_buffer_ = context_.create_buffer_and_upload(
        &host_data_, sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      );
    }

    /* Initialize the skybox and setup the HDR diffuse envmap used to calculate
     * IBL convolutions. */
    auto &skybox = renderer_.skybox();
    skybox.setup(ASSETS_DIR "textures/qwantani_dusk_2_2k.hdr");

    /* Load glTF Scene / Resources. */
    {
      std::string const gltf_filename{ASSETS_DIR "models/"
        "suzanne.glb"
      };

      /* Load the model directly on device, as we do not change the model's internal data
       * layout we need to specify how to map its attributes to the shader used. */
      scene_ = renderer_.load_and_upload(gltf_filename, {
        { Geometry::AttributeType::Position,  shader_interop::kAttribLocation_Position },
        { Geometry::AttributeType::Texcoord,  shader_interop::kAttribLocation_Texcoord },
        { Geometry::AttributeType::Normal,    shader_interop::kAttribLocation_Normal   },
      });

      LOG_CHECK(scene_->device_images.size() <= kMaxNumTextures); //
    }

    /* Release the temporary staging buffers. */
    allocator_ptr_ = context_.allocator_ptr();
    allocator_ptr_->clear_staging_buffers();

    /* Descriptor set. */
    {
      descriptor_set_layout_ = renderer_.create_descriptor_set_layout({
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
        },
        {
          .binding = shader_interop::kDescriptorSetBinding_IrradianceEnvMap,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1u,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        },
      });

      descriptor_set_ = renderer_.create_descriptor_set(descriptor_set_layout_, {
        {
          .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .buffers = { { uniform_buffer_.buffer } },
        },
        {
          .binding = shader_interop::kDescriptorSetBinding_IrradianceEnvMap,
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .images = {
            {
              .sampler = renderer_.get_default_sampler(),
              .imageView = skybox.irradiance_map().view,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }
          },
        },
      });
    }

    /* Update the Sampler Atlas descriptor with the currently loaded textures. */
    context_.update_descriptor_set(descriptor_set_, {
      scene_->descriptor_set_texture_atlas_entry( shader_interop::kDescriptorSetBinding_Sampler )
    });

    auto shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "simple.vert.glsl",
      "simple.frag.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      VkPipelineLayout const pipeline_layout = renderer_.create_pipeline_layout({
        .setLayouts = { descriptor_set_layout_ },
        .pushConstantRanges = {
          {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                        | VK_SHADER_STAGE_FRAGMENT_BIT
                        ,
            .size = sizeof(shader_interop::PushConstant),
          }
        },
      });

      graphics_pipeline_ = renderer_.create_graphics_pipeline(pipeline_layout, {
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
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        },
        .primitive = {
          .cullMode = VK_CULL_MODE_BACK_BIT,
        }
      });
    }

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    renderer_.destroy_descriptor_set_layout(descriptor_set_layout_);
    renderer_.destroy_pipeline_layout(graphics_pipeline_.get_layout());
    renderer_.destroy_pipeline(graphics_pipeline_);
    allocator_ptr_->destroy_buffer(uniform_buffer_);
    scene_.reset();
  }

  void update_frame(float const delta_time) {
    camera_.update( delta_time );
    push_constant_.viewMatrix = camera_.view();
  }

  void draw_model(RenderPassEncoder const& pass, mat4 const& world_matrix) {
    for (auto const& mesh : scene_->meshes) {
      pass.set_primitive_topology(mesh->vk_primitive_topology());
      push_constant_.model.worldMatrix = linalg::mul(
        world_matrix,
        mesh->world_matrix()
      );
      for (auto const& submesh : mesh->submeshes) {
        auto const& material_ref = *(submesh.material_ref);
        if (auto *fx = scene_->material_fx<fx::material::PBRMetallicRoughnessFx>(material_ref); fx) {
          auto pbr_material = fx->material(material_ref.material_index);
          push_constant_.model.albedo_texture_index = pbr_material.diffuse_texture_id;
        }
        pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); //
        pass.draw(submesh.draw_descriptor, scene_->vertex_buffer, scene_->index_buffer);
      }
    }
  }

  void frame() final {
    update_frame(delta_time());

    mat4 const worldMatrix{
      // linalg::identity
      lina::rotation_matrix_axis(
        vec3(-0.25f, 1.0f, -0.15f),
        frame_time() * 0.4f
      )
    };

    auto cmd = renderer_.begin_frame();
    {
      auto pass = cmd.begin_rendering();
      {
        pass.set_viewport_scissor(viewport_size_);

        /* First render the skybox. */
        renderer_.skybox().render(pass, camera_);

        /* Then the scene / model. */
        pass.bind_pipeline(graphics_pipeline_);
        {
          pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT);
          draw_model(pass, worldMatrix);
        }
      }
      cmd.end_rendering();
    }
    renderer_.end_frame();
  }

 private:
  ResourceAllocator* allocator_ptr_{};

  HostData_t host_data_{};
  backend::Buffer uniform_buffer_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::PushConstant push_constant_{};

  Pipeline graphics_pipeline_{};

  // -----------------------

  Camera camera_{};
  ArcBallController arcball_controller_{};

  GLTFScene scene_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
