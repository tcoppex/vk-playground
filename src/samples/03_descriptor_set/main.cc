/* -------------------------------------------------------------------------- */
//
//    03 - Hello Descriptor Set
//
//  Demonstrate a very simple use of a descriptor set, in 3D !
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/renderer/graphics_pipeline.h"

namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  static bool constexpr kFlipScreenVertically{ true };

  /* Shortcut to the uniform data type used. */
  using HostData_t = shader_interop::UniformData;

  struct Vertex_t {
    float Position[4];
    float Normal[3];
  };

  enum AttributeLocation {
    Position = 0,
    Normal   = 1,
    kAttributeLocationCount
  };

  std::vector<Vertex_t> const kVertices{
    {.Position = { 1.0f, 1.0f, 1.0f, 1.0f}, .Normal = { 0.577,  0.577,  0.577}},
    {.Position = {-1.0f,-1.0f, 1.0f, 1.0f}, .Normal = {-0.577, -0.577,  0.577}},
    {.Position = {-1.0f, 1.0f,-1.0f, 1.0f}, .Normal = {-0.577,  0.577, -0.577}},
    {.Position = { 1.0f,-1.0f,-1.0f, 1.0f}, .Normal = { 0.577, -0.577, -0.577}},
  };

  std::vector<uint16_t> const kIndices{
    0, 2, 1,  0, 1, 3,
    0, 3, 2,  1, 2, 3
  };

 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    wm_->setTitle("03 - Πρίσμα");

    renderer_.set_color_clear_value({.float32 = {0.125f, 0.125f, 0.125f, 1.0f}});

    allocator_ = context_.get_resource_allocator();

    /* Initialize the scene data on the host, here just the camera matrices. */
    {
      host_data_.scene.camera = {
        .viewMatrix = linalg::lookat_matrix(
          vec3f(1.0f, 2.0f, 5.0f),
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

    /* Create & upload Uniform, Vertex & Index buffers on the device. */
    {
      auto cmd = context_.create_transient_command_encoder();

      uniform_buffer_ = cmd.create_buffer_and_upload(
        &host_data_, sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      );

      vertex_buffer_ = cmd.create_buffer_and_upload(
        kVertices,
        VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
      );

      index_buffer_ = cmd.create_buffer_and_upload(
        kIndices,
        VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT
      );

      context_.finish_transient_command_encoder(cmd);
    }

    /* Create the descriptor set with its binding. */
    {
      uint32_t const kDescSetUniformBinding = 0u;

      descriptor_set_layout_ = renderer_.create_descriptor_set_layout({
        .entries = {
          {
            .binding = kDescSetUniformBinding,
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

      /**
       * Create a descriptor set from a layout and update it directly (optionnal).
       * The descriptor set can be update later on by calling 'update_descriptor_set'.
       **/
      descriptor_set_ = renderer_.create_descriptor_set(descriptor_set_layout_, {
        {
          .binding = kDescSetUniformBinding,
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .resource = { .buffer = { uniform_buffer_.buffer } }
        }
      });
    }

    auto shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "vs_simple.glsl",
      "fs_simple.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      auto& gp = graphics_pipeline_;

      /**
       * Add the descriptor set layout use by this pipeline (here just one).
       **/
      VkPipelineLayout const pipeline_layout = renderer_.create_pipeline_layout({
        .setLayouts = { descriptor_set_layout_ },
        .pushConstantRanges = {
          {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size = sizeof(shader_interop::PushConstant),
          }
        },
      });
      gp.set_pipeline_layout(pipeline_layout);

      gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
      gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[1u]);

      /**
       * By default graphics pipeline expect mesh data layout as Triangle List,
       * but we can change it using 'set_topology'.
       **/
      gp.set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

      gp.set_vertex_binding_attribute({
        .bindings = {
          {
            .binding = 0u,
            .stride = sizeof(Vertex_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
          },
        },
        .attributes = {
          {
            .location = AttributeLocation::Position,
            .binding = 0u,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex_t, Vertex_t::Position),
          },
          {
            .location = AttributeLocation::Normal,
            .binding = 0u,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex_t, Vertex_t::Normal),
          },
        },
      });

      gp.complete(context_.get_device(), renderer_);
    }

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    renderer_.destroy_pipeline_layout(graphics_pipeline_.get_layout());
    renderer_.destroy_descriptor_set_layout(descriptor_set_layout_);

    graphics_pipeline_.release(context_.get_device());

    allocator_->destroy_buffer(index_buffer_);
    allocator_->destroy_buffer(vertex_buffer_);
    allocator_->destroy_buffer(uniform_buffer_);
  }

  void frame() final {
    /* Update the model world matrix. */
    {
      float const frame_time{ get_frame_time() };
      auto const axis{
        vec3f(0.2f * cosf(3.0f*frame_time), 0.8f, sinf(frame_time))
      };
      push_constant_.model.worldMatrix = linalg::rotation_matrix(
        linalg::rotation_quat(linalg::normalize(axis), frame_time * 0.75f)
      );
    }

    auto cmd = renderer_.begin_frame();
    {
      auto pass = cmd.begin_rendering();
      {
        pass.set_viewport_scissor(viewport_size_, kFlipScreenVertically);

        pass.set_pipeline(graphics_pipeline_);
        pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT);

        /**
         * We need to bind the descriptor set(s) used by each pipeline layout in
         * used.
         *
         * Like push_constant, if a pipeline is currently bound, the RenderPassEncoder
         * will automaticaly use their's as the targeted pipeline layout.
         **/
        pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT);

        pass.set_vertex_buffer(vertex_buffer_);

        /**
         * The 'set_index_buffer' function specifies the buffer from which indices
         * are retrieved during 'draw_indexed' operations. By default, it expects
         * an index buffer with 32-bit unsigned integers (uint32).
         *
         * The second parameter allows you to specify a different index type,
         * such as VK_INDEX_TYPE_UINT16, for compatibility with smaller index formats.
         */
        pass.set_index_buffer(index_buffer_, VK_INDEX_TYPE_UINT16);
        pass.draw_indexed(kIndices.size());
      }
      cmd.end_rendering();
    }
    renderer_.end_frame();
  }

 private:
  std::shared_ptr<ResourceAllocator> allocator_;

  HostData_t host_data_{};
  Buffer_t uniform_buffer_;

  Buffer_t vertex_buffer_;
  Buffer_t index_buffer_;

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};

  GraphicsPipeline graphics_pipeline_;

  shader_interop::PushConstant push_constant_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
