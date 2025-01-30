/* -------------------------------------------------------------------------- */
//
//    05 - Hello Stencil Op
//
//  Where we look into the abyss via a multi passes stencil effect.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/utils/geometry.h"

namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  using HostData_t = shader_interop::UniformData;

  static constexpr float kPortalSize = 2.35f;

  /* Helper to store unique static mesh resources. */
  struct Mesh_t {
    Geometry geo;
    Buffer_t vertex;
    Buffer_t index;
  };

  enum class PipelineID : uint32_t {
    StencilMask,
    StencilTest,
    DepthMask,
    Rendering,
    kCount
  };

 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    wm_->setTitle("05 - přemýšlet s portály");

    renderer_.set_color_clear_value({{ 0.1078f, 0.1079f, 0.1081f, 1.0f }});

    allocator_ = context_.get_resource_allocator();

    /* Initialize the scene data. */
    {
      host_data_.scene.camera = {
        .viewMatrix = linalg::lookat_matrix(
          vec3f(0.0f, 0.0f, 4.0f),
          vec3f(0.0f, 0.0f, 0.0f),
          vec3f(0.0f, 1.0f, 0.0f)
        ),
        .projectionMatrix = linalg::perspective_matrix(
          lina::radians(60.0f),
          static_cast<float>(viewport_size_.width) / static_cast<float>(viewport_size_.height),
          0.01f,
          500.0f,
          linalg::neg_z,
          linalg::zero_to_one
        ),
      };
    }

    /* Create Uniform, Vertices, and Indices Buffers. */
    {
      auto cmd = context_.create_transient_command_encoder();

      uniform_buffer_ = cmd.create_buffer_and_upload(
        &host_data_, sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      );

      // Plane
      {
        auto &mesh = plane_;
        Geometry::MakePlane(mesh.geo, kPortalSize);

        mesh.vertex = cmd.create_buffer_and_upload(
          mesh.geo.get_vertices(),
          VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
        );
        mesh.index = cmd.create_buffer_and_upload(
          mesh.geo.get_indices(),
          VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT
        );
      }

      // Torus
      {
        auto &mesh = torus_;
        float const r = 0.5f * kPortalSize;
        Geometry::MakeTorus2(mesh.geo, r, r * lina::kSqrtTwo, 48u, 32u);

        mesh.vertex = cmd.create_buffer_and_upload(
          mesh.geo.get_vertices(),
          VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
        );
        mesh.index = cmd.create_buffer_and_upload(
          mesh.geo.get_indices(),
          VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT
        );
      }

      context_.finish_transient_command_encoder(cmd);
    }

    /* Descriptor set. */
    {
      descriptor_set_layout_ = renderer_.create_descriptor_set_layout({
        .entries = {
          {
            .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1u,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          },
        },
        .flags = {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
          | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
          | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
          ,
        },
      });

      descriptor_set_ = renderer_.create_descriptor_set(descriptor_set_layout_, {
        {
          .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .resource = { .buffer = { uniform_buffer_.buffer } }
        },
      });
    }

    /* Create a Pipeline Layout to be shared. */
    pipeline_layout_ = renderer_.create_pipeline_layout({
      .setLayouts = { descriptor_set_layout_ },
      .pushConstantRanges = {
        {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .size = sizeof(shader_interop::PushConstant),
        }
      },
    });

    auto shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "vs_simple.glsl",
      "vs_instanced.glsl",
      "fs_simple.glsl",
    })};

    /* Setup the pipelines. */
    {
      /* "Shared" descriptor between the stencil and the depth mask pipelines. */
      GraphicsPipelineDescriptor_t mask_pipeline_descriptor{
        .vertex = {
          .module = shaders[0u].module,
          .buffers = {
            {
              .stride = plane_.geo.get_stride(),
              .attributes =  plane_.geo.get_vk_binding_attributes(0u, {
                { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
                { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
                { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
              }),
            }
          }
        },
        .fragment = {
          .module = shaders[2u].module,
          .targets = {
            {
              .format = renderer_.get_color_attachment().format,
              .writeMask = 0u,
            }
          },
        },
        .depthStencil = {
          .format = renderer_.get_depth_stencil_attachment().format,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_FALSE,
          .depthCompareOp = VK_COMPARE_OP_ALWAYS,
          .stencilTestEnable = VK_TRUE,
          .stencilFront = {
            .failOp = VK_STENCIL_OP_REPLACE,
            .passOp = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_REPLACE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0xff,
            .writeMask = 0xff,
            .reference = 1u,
          },
        },
        .primitive = {
          .topology = plane_.geo.get_vk_primitive_topology(),
          .cullMode = VK_CULL_MODE_BACK_BIT,
        }
      };

      /* Write inside the stencil buffer. */
      pipelines_[PipelineID::StencilMask] = renderer_.create_graphics_pipeline(pipeline_layout_, mask_pipeline_descriptor);

      /* Render inside the portal using the stencil test and the instancing vertex shader. */
      pipelines_[PipelineID::StencilTest] = renderer_.create_graphics_pipeline(pipeline_layout_, {
        .vertex = {
          .module = shaders[1u].module,
          .buffers = {
            {
              .stride = torus_.geo.get_stride(),
              .attributes =  torus_.geo.get_vk_binding_attributes(0u, {
                { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
                { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
                { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
              }),
            }
          }
        },
        .fragment = {
          .module = shaders[2u].module,
          .targets = {
            {
              .format = renderer_.get_color_attachment().format,
              .writeMask = VK_COLOR_COMPONENT_R_BIT
                         | VK_COLOR_COMPONENT_G_BIT
                         | VK_COLOR_COMPONENT_B_BIT
                         | VK_COLOR_COMPONENT_A_BIT
                         ,
            }
          },
        },
        .depthStencil = {
          .format = renderer_.get_depth_stencil_attachment().format,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
          .stencilTestEnable = VK_TRUE,
          .stencilFront = {
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_EQUAL,
            .compareMask = 0xFF,
            .writeMask = 0xFF,
            .reference = 1u,
          },
        },
        .primitive = {
          .topology = torus_.geo.get_vk_primitive_topology(),
          .cullMode = VK_CULL_MODE_BACK_BIT,
        }
      });

      /* Only write in the depth buffer, to mask the portal once we've rendered into it. */
      mask_pipeline_descriptor.depthStencil.depthWriteEnable = VK_TRUE;
      pipelines_[PipelineID::DepthMask] = renderer_.create_graphics_pipeline(pipeline_layout_, mask_pipeline_descriptor);

      /* Basic rendering pipeline, do not use the stencil. */
      pipelines_[PipelineID::Rendering] = renderer_.create_graphics_pipeline(pipeline_layout_, {
        .vertex = {
          .module = shaders[0u].module,
          .buffers = {
            {
              .stride = torus_.geo.get_stride(),
              .attributes =  torus_.geo.get_vk_binding_attributes(0u, {
                { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
                { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
                { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
              }),
            }
          }
        },
        .fragment = {
          .module = shaders[2u].module,
          .targets = {
            {
              .format = renderer_.get_color_attachment().format,
              .writeMask = VK_COLOR_COMPONENT_R_BIT
                         | VK_COLOR_COMPONENT_G_BIT
                         | VK_COLOR_COMPONENT_B_BIT
                         | VK_COLOR_COMPONENT_A_BIT
                         ,
            }
          },
        },
        .depthStencil = {
          .format = renderer_.get_depth_stencil_attachment().format,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        },
        .primitive = {
          .topology = torus_.geo.get_vk_primitive_topology(),
          .cullMode = VK_CULL_MODE_BACK_BIT,
        }
      });
    }

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    for (auto const& pipeline : pipelines_) {
      renderer_.destroy_pipeline(pipeline);
    }
    renderer_.destroy_descriptor_set_layout(descriptor_set_layout_);
    renderer_.destroy_pipeline_layout(pipeline_layout_);

    allocator_->destroy_buffer(plane_.index);
    allocator_->destroy_buffer(plane_.vertex);

    allocator_->destroy_buffer(torus_.index);
    allocator_->destroy_buffer(torus_.vertex);

    allocator_->destroy_buffer(uniform_buffer_);
  }

  void frame() final {
    float const frame_time{ get_frame_time() };

    mat4 const portal_world_matrix = linalg::mul(
      lina::rotation_matrix_x(lina::kHalfPi),
      lina::rotation_matrix_axis(
        vec3(2.45f*cosf(0.5f*frame_time), 1.35f, -1.4*sinf(0.35f*frame_time)),
        0.35f*frame_time
      )
    );

    auto cmd = renderer_.begin_frame();
    {
      /* As the pipeline shared the same layout, we can bind them just once directly. */
      cmd.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT);

      /* All objects but the last one use the same world matrix. */
      push_constant_.model.worldMatrix = portal_world_matrix;
      cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT);

      auto pass = cmd.begin_rendering();
      {
        pass.set_viewport_scissor(viewport_size_);

        // Write the portal mask into the stencil buffer.
        {
          auto &mesh = plane_;
          pass.set_pipeline(pipelines_[PipelineID::StencilMask]);

          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count());
        }

        // Instanced rings 'behind' the stencil mask.
        {
          auto &mesh = torus_;
          pass.set_pipeline(pipelines_[PipelineID::StencilTest]);

          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count(), 256u);
        }

        // Write the portal mask into the depth buffer.
        {
          auto &mesh = plane_;
          pass.set_pipeline(pipelines_[PipelineID::DepthMask]);

          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count());
        }

        // Render objects 'outside' the portal.
        {
          auto &mesh = torus_;
          pass.set_pipeline(pipelines_[PipelineID::Rendering]);

          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count());

          push_constant_.model.worldMatrix = linalg::mul(
            linalg::scaling_matrix(vec3(3.0)),
            lina::rotation_matrix_z(-0.32f*frame_time)
          );
          pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT);
          pass.draw_indexed(mesh.geo.get_index_count());
        }
      }
      cmd.end_rendering();
    }
    renderer_.end_frame();
  }

 private:
  std::shared_ptr<ResourceAllocator> allocator_;

  HostData_t host_data_{};
  Buffer_t uniform_buffer_{};

  Mesh_t plane_;
  Mesh_t torus_;

  VkPipelineLayout pipeline_layout_{};
  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::PushConstant push_constant_{};

  EnumArray<Pipeline, PipelineID> pipelines_;
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
