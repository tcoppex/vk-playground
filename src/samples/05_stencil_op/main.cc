/* -------------------------------------------------------------------------- */
//
//    05 - Hello Stencil Op
//
//  Where we look into the abyss via a multi passes stencil effect.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/renderer/graphics_pipeline.h"
#include "framework/utils/geometry.h"

namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  using HostData_t = shader_interop::UniformData;

  /* Helper to store unique static mesh resources. */
  struct Mesh_t {
    Geometry geo;
    Buffer_t vertex;
    Buffer_t index;
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

    /* Create Buffers. */
    {
      auto cmd = context_.create_transient_command_encoder();

      uniform_buffer_ = cmd.create_buffer_and_upload(
        &host_data_, sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      );

      // [TODO] Better device buffer management (eg. shared buffers for statics).
      //------------------------------------------
      float const plane_size = 2.0f;
      // Plane
      {
        auto &mesh = plane_;
        Geometry::MakePlane(mesh.geo, plane_size);

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
        float const r = 0.5f * plane_size;
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
      //------------------------------------------

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
      {
        auto& gp = stencil_mask_;
        auto& mesh = plane_;

        gp.set_pipeline_layout(pipeline_layout_);
        gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
        gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[2u]);
        gp.set_topology(mesh.geo.get_vk_primitive_topology());
        gp.set_vertex_binding_attribute({
          .bindings = {
            {
              .binding = 0u,
              .stride = mesh.geo.get_stride(),
              .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
          },
          .attributes = mesh.geo.get_vk_binding_attributes(0u, {
            { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
            { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
            { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
          }),
        });

        /* We will only write on one side of the portal, so the plane mask is cull on its back. */
        gp.set_cull_mode(VK_CULL_MODE_BACK_BIT);

        // -------------------------------------------
        auto & depth_stencil = gp.get_states().depth_stencil;

        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_FALSE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

        depth_stencil.stencilTestEnable = VK_TRUE;
        depth_stencil.front = {
          .failOp = VK_STENCIL_OP_REPLACE,
          .passOp = VK_STENCIL_OP_REPLACE,
          .depthFailOp = VK_STENCIL_OP_REPLACE,
          .compareOp = VK_COMPARE_OP_ALWAYS,
          .compareMask = 0xff,
          .writeMask = 0xff,
          .reference = 1u,
        };
        depth_stencil.back = depth_stencil.front;

        // Do not write the mask into the color buffer.
        auto & color_blend = gp.get_color_blend_attachment();
        color_blend.colorWriteMask = 0;
        // -------------------------------------------

        gp.complete(context_.get_device(), renderer_);
      }


      {
        auto& gp = stencil_test_;
        auto& mesh = torus_;

        gp.set_pipeline_layout(pipeline_layout_);
        gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[1u]);
        gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[2u]);
        gp.set_topology(mesh.geo.get_vk_primitive_topology());
        gp.set_vertex_binding_attribute({
          .bindings = {
            {
              .binding = 0u,
              .stride = mesh.geo.get_stride(),
              .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
          },
          .attributes = mesh.geo.get_vk_binding_attributes(0u, {
            { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
            { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
            { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
          }),
        });
        gp.set_cull_mode(VK_CULL_MODE_BACK_BIT);

        // -------------------------------------------
        auto & depth_stencil = gp.get_states().depth_stencil;
        depth_stencil.stencilTestEnable = VK_TRUE;
        depth_stencil.front = {
          .failOp = VK_STENCIL_OP_KEEP,
          .passOp = VK_STENCIL_OP_KEEP,
          .depthFailOp = VK_STENCIL_OP_KEEP,
          .compareOp = VK_COMPARE_OP_EQUAL,
          .compareMask = 0xFF,
          .writeMask = 0xff,
          .reference = 1u,
        };
        depth_stencil.back = {};
        // -------------------------------------------

        gp.complete(context_.get_device(), renderer_);
      }

      // !! exactly the same as 'stencil_mask' but with depthWrite enable !!
      {
        auto& gp = depth_mask_;
        auto& mesh = plane_;

        gp.set_pipeline_layout(pipeline_layout_);
        gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
        gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[2u]);
        gp.set_topology(mesh.geo.get_vk_primitive_topology());
        gp.set_vertex_binding_attribute({
          .bindings = {
            {
              .binding = 0u,
              .stride = mesh.geo.get_stride(),
              .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
          },
          .attributes = mesh.geo.get_vk_binding_attributes(0u, {
            { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
            { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
            { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
          }),
        });

        gp.set_cull_mode(VK_CULL_MODE_BACK_BIT);

        // -------------------------------------------
        auto & depth_stencil = gp.get_states().depth_stencil;

        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

        depth_stencil.stencilTestEnable = VK_TRUE;
        depth_stencil.front = {
          .failOp = VK_STENCIL_OP_REPLACE,
          .passOp = VK_STENCIL_OP_REPLACE,
          .depthFailOp = VK_STENCIL_OP_REPLACE,
          .compareOp = VK_COMPARE_OP_ALWAYS,
          .compareMask = 0xff,
          .writeMask = 0xff,
          .reference = 1u,
        };
        depth_stencil.back = depth_stencil.front;

        auto & color_blend = gp.get_color_blend_attachment();
        color_blend.colorWriteMask = 0;
        // -------------------------------------------

        gp.complete(context_.get_device(), renderer_);
      }

      {
        auto& gp = render_pipeline_;
        auto& mesh = torus_;

        gp.set_pipeline_layout(pipeline_layout_);
        gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
        gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[2u]);
        gp.set_topology(mesh.geo.get_vk_primitive_topology());
        gp.set_vertex_binding_attribute({
          .bindings = {
            {
              .binding = 0u,
              .stride = mesh.geo.get_stride(),
              .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
          },
          .attributes = mesh.geo.get_vk_binding_attributes(0u, {
            { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
            { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
            { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
          }),
        });
        gp.set_cull_mode(VK_CULL_MODE_BACK_BIT);

        gp.complete(context_.get_device(), renderer_);
      }


      /* Demonstrate dynamic pipeline. */
      {
        auto& gp = dynamic_pipeline_;
        auto& mesh = plane_;

        gp.set_pipeline_layout(pipeline_layout_);

        gp.add_dynamic_states({
          VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
          VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
          VK_DYNAMIC_STATE_STENCIL_REFERENCE,

          VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
          VK_DYNAMIC_STATE_STENCIL_OP_EXT,

          VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
          VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,

          VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
          VK_DYNAMIC_STATE_CULL_MODE_EXT,
        });

        gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
        gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[2u]);
        gp.set_vertex_binding_attribute({
          .bindings = {
            {
              .binding = 0u,
              .stride = mesh.geo.get_stride(),
              .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
          },
          .attributes = mesh.geo.get_vk_binding_attributes(0u, {
            { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
            { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
            { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
          }),
        });

        gp.complete(context_.get_device(), renderer_);
      }

    }

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    auto device = context_.get_device();
    stencil_test_.release(device);
    stencil_mask_.release(device);
    depth_mask_.release(device);
    render_pipeline_.release(device);

    dynamic_pipeline_.release(device);

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

      /* Almost all object but the last one use the same world matrix. */
      push_constant_.model.worldMatrix = portal_world_matrix;
      cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT);

      auto pass = cmd.begin_rendering();
      {
        pass.set_viewport_scissor(viewport_size_);

        // Write the portal mask into the stencil buffer.
        pass.set_pipeline(stencil_mask_);
        {
          auto &mesh = plane_;
          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count());
        }

        // Instanced rings 'behind' the stencil mask.
        pass.set_pipeline(stencil_test_);
        {
          auto &mesh = torus_;
          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count(), 256);
        }



        // Write the portal mask into the depth buffer.
#if 1
        pass.set_pipeline(depth_mask_);
        {
          auto &mesh = plane_;
          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count());
        }
#else
        pass.set_pipeline(dynamic_pipeline_);
        {
          {
            auto cmdbuf = pass.get_handle();

            vkCmdSetDepthWriteEnableEXT(cmdbuf, VK_TRUE);

            vkCmdSetStencilTestEnableEXT(cmdbuf, VK_TRUE);
            vkCmdSetStencilOpEXT(cmdbuf,
              VK_STENCIL_FACE_FRONT_AND_BACK,
              VK_STENCIL_OP_REPLACE,
              VK_STENCIL_OP_REPLACE,
              VK_STENCIL_OP_REPLACE,
              VK_COMPARE_OP_ALWAYS
            );
            vkCmdSetStencilCompareMask(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
            vkCmdSetStencilWriteMask(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
            vkCmdSetStencilReference(cmdbuf, VK_STENCIL_FACE_FRONT_AND_BACK, 1u);

            vkCmdSetCullModeEXT(cmdbuf, VK_CULL_MODE_BACK_BIT);

            VkColorComponentFlags colorWrite(0);
            vkCmdSetColorWriteMaskEXT(cmdbuf, 0u, 1u, &colorWrite);

            vkCmdSetPrimitiveTopologyEXT(cmdbuf, plane_.geo.get_vk_primitive_topology());
          }

          auto &mesh = plane_;
          pass.set_vertex_buffer(mesh.vertex);
          pass.set_index_buffer(mesh.index, mesh.geo.get_vk_index_type());
          pass.draw_indexed(mesh.geo.get_index_count());
        }
#endif

        // Render objects 'outside' the portal.
        pass.set_pipeline(render_pipeline_);
        {
          auto &mesh = torus_;
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

  VkPipelineLayout pipeline_layout_{};
  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::PushConstant push_constant_{};

  GraphicsPipeline stencil_mask_{};
  GraphicsPipeline stencil_test_{};
  GraphicsPipeline depth_mask_{};
  GraphicsPipeline render_pipeline_{};

  GraphicsPipeline dynamic_pipeline_{};

  // -------------
  Mesh_t plane_;
  Mesh_t torus_;
  // -------------
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
