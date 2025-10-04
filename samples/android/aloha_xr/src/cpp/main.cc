/* -------------------------------------------------------------------------- */
//
//    02 - Aloha XR
//
//   Show a simple XR demo, the verbose way.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"

namespace shader_interop {
#include "../shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
private:
  struct Vertex_t {
    vec4 Position;
    vec4 Color;
  };

  enum AttributeLocation : uint8_t {
    Position = 0,
    Color    = 1,
  };

  std::vector<Vertex_t> const kVertices{
    {.Position = { -1.0f, -1.0f, +0.0f, 1.0f}, .Color = {0.5f, 0.2f, 1.0f, 1.0f}},
    {.Position = { +1.0f, -1.0f, +0.0f, 1.0f}, .Color = {1.0f, 0.5f, 0.2f, 1.0f}},
    {.Position = {   0.0f, +1.0f, +0.0f, 1.0f}, .Color = {0.2f, 1.0f, 0.5f, 1.0f}},
  };

  static
  std::array<vec4, 9u> constexpr kColors{
    vec4(0.89f, 0.45f, 0.00f, 1.0f),
    vec4(0.52f, 0.26f, 0.75f, 1.0f),
    vec4(0.48f, 0.60f, 0.61f, 1.0f),
    vec4(0.43f, 0.87f, 0.62f, 1.0f),
    vec4(0.98f, 0.98f, 0.98f, 1.0f),
    vec4(0.96f, 0.38f, 0.32f, 1.0f),
    vec4(0.24f, 0.16f, 0.09f, 1.0f),
    vec4(0.96f, 0.51f, 0.66f, 1.0f),
    vec4(0.70f, 0.85f, 0.45f, 1.0f),
  };

 public:
  SampleApp() = default;

 private:
  bool setup() final {
    renderer_.set_color_clear_value(vec4(0.715f, 0.305f, 0.195f, 1.0f));

    vertex_buffer_ = context_.create_buffer_and_upload(
      kVertices,
      VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
    );

    uniform_buffer_ = context_.allocator().create_buffer(
      2u * sizeof(shader_interop::UniformCameraData),
      VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
    );

    /* Create the descriptor set with its binding. */
    {
      uint32_t const kDescSetUniformBinding = 0u;

      descriptor_set_layout_ = context_.create_descriptor_set_layout({{
        .binding = kDescSetUniformBinding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 2u,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                      ,
      }});

      descriptor_set_ = context_.create_descriptor_set(descriptor_set_layout_, {{
        .binding = kDescSetUniformBinding,
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .buffers = { { uniform_buffer_.buffer } }
      }});
    }

    auto const shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "simple.vert",
      "simple.frag",
    })};

    pipeline_layout_ = context_.create_pipeline_layout({
      .setLayouts = { descriptor_set_layout_ },
      .pushConstantRanges = {
        {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .size = sizeof(shader_interop::PushConstant),
        }
      },
    });

    graphics_pipeline_ = renderer_.create_graphics_pipeline(
      pipeline_layout_,
      {
        .vertex = {
          .module = shaders[0u].module,
          .buffers = {
            {
              .stride = sizeof(Vertex_t),
              .attributes =  {
                {
                  .location = AttributeLocation::Position,
                  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                  .offset = offsetof(Vertex_t, Position),
                },
                {
                  .location = AttributeLocation::Color,
                  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                  .offset = offsetof(Vertex_t, Color),
                },
              }
            }
          }
        },
        .fragment = {
          .module = shaders[1u].module,
          .targets = {
            {
              .format = renderer_.color_attachment().format,
              .writeMask = VK_COLOR_COMPONENT_R_BIT
                         | VK_COLOR_COMPONENT_G_BIT
                         | VK_COLOR_COMPONENT_B_BIT
                         | VK_COLOR_COMPONENT_A_BIT
                         ,
            }
          },
        },
        .depthStencil = {
          .format = renderer_.depth_stencil_attachment().format,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        },
        .primitive = {
          .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .cullMode = VK_CULL_MODE_NONE,
        }
      }
    );

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    context_.destroyResources(
      descriptor_set_layout_,
      pipeline_layout_,
      graphics_pipeline_
    );
    context_.allocator().destroy_buffer(uniform_buffer_);
    context_.allocator().destroy_buffer(vertex_buffer_);
  }

  void update_xr(XRFrameData_t const& frame) final {
    std::vector<shader_interop::UniformCameraData> cameraBuffer{
      {
        .projectionMatrix = frame.projMatrices[0],
        .viewMatrix = frame.viewMatrices[0]
      },
      {
        .projectionMatrix = frame.projMatrices[1],
        .viewMatrix = frame.viewMatrices[1]
      },
    };
    context_.upload_buffer(cameraBuffer, uniform_buffer_);

    {
      auto const& input = *frame.inputs;
      if (input.button_a || input.button_x) {
        space_id_ = static_cast<XRSpaceId>(
          (space_id_ == 0u) ? frame.spaceMatrices.size() - 1u : space_id_ - 1
        );
      }
      if (input.button_b || input.button_y) {
        space_id_ = static_cast<XRSpaceId>(
          (space_id_ >= frame.spaceMatrices.size()-1u) ? 0u : space_id_ + 1
        );
      }
      if (auto *ptr = frame.spaceMatrices[space_id_]; nullptr != ptr) {
        push_constant_.modelMatrix = *ptr;
      }
    }

    // Calculate model matrix.
    {
      mat4f const translateMatrix = linalg::translation_matrix(float3{0.0f, 0.0f, -3.0f});
      push_constant_.modelMatrix = linalg::mul(push_constant_.modelMatrix, translateMatrix);
      // mat4f const rotationMatrix = linalg::rotation_matrix(linalg::rotation_quat(vec3f(0.0f, 1.0f, 0.0f), angle));
      // modelMatrix_ = linalg::mul(spaceModelMatrix_, modelMatrix_);
    }
  }

  void draw() final {
    auto cmd = renderer_.begin_frame();

    auto pass = cmd.begin_rendering();
    {
      // LOGI("{} {}", viewport_size_.width, viewport_size_.height);

      // (in multivew we probably have to set up two viewport scissor..)
      //-------------
      // pass.set_viewport_scissor(viewport_size_); // <<< not update on XR
      // pass.set_viewport_scissor(renderer_.surface_size());//
      //-------------

      pass.bind_pipeline(graphics_pipeline_);
      pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT);
      pass.bind_vertex_buffer(vertex_buffer_);

      pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT);

      pass.draw(kVertices.size());
    }
    cmd.end_rendering();

    renderer_.end_frame();
  }

 private:
  backend::Buffer vertex_buffer_{};
  backend::Buffer uniform_buffer_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};

  VkPipelineLayout pipeline_layout_{};
  Pipeline graphics_pipeline_{};

  shader_interop::PushConstant push_constant_{};
  XRSpaceId space_id_{XRSpaceId::Local};

  // uint32_t clear_color_index_{};
};

// ----------------------------------------------------------------------------

ENTRY_POINT(SampleApp)

/* -------------------------------------------------------------------------- */
