#ifndef HELLOVK_FRAMEWORK_RENDERER_PIPELINE_H
#define HELLOVK_FRAMEWORK_RENDERER_PIPELINE_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/types.h"

class Renderer;

// ----------------------------------------------------------------------------

class Pipeline : public PipelineInterface {
 public:
  Pipeline() = default;
  ~Pipeline() = default;

  Pipeline(VkPipelineLayout layout, VkPipeline pipeline, bool use_internal_layout = false)
   : PipelineInterface(layout, pipeline)
   , use_internal_layout_{use_internal_layout}
  {}

 private:
  bool use_internal_layout_{};

  friend class Renderer;
};

// ----------------------------------------------------------------------------

struct PipelineLayoutDescriptor_t {
  std::vector<VkDescriptorSetLayout> setLayouts{};
  std::vector<VkPushConstantRange> pushConstantRanges{};
};

// ----------------------------------------------------------------------------

// Descriptor structure to create GraphicsPipeline, à la WebGPU.
struct GraphicsPipelineDescriptor_t {
  std::vector<VkDynamicState> dynamicStates{};

  struct Vertex {
    struct Buffer {
      uint32_t stride{};
      VkVertexInputRate inputRate{};
      std::vector<VkVertexInputAttributeDescription> attributes{};
    };

    VkShaderModule module{}; //
    std::string entryPoint{};
    std::vector<Buffer> buffers{};
  } vertex{};

  struct Fragment {
    struct Target {
      VkFormat format{};
      VkColorComponentFlags writeMask{};

      struct Blending {
        struct Parameters {
          VkBlendOp operation{};
          VkBlendFactor srcFactor{};
          VkBlendFactor dstFactor{};
        };

        VkBool32 enable{};
        Parameters color{};
        Parameters alpha{};
      } blend{};
    };

    VkShaderModule module{};
    std::string entryPoint{};
    std::vector<Target> targets{};
  } fragment{};

  struct DepthStencil {
    VkFormat format{}; //

    VkBool32 depthTestEnable{};
    VkBool32 depthWriteEnable{};
    VkCompareOp depthCompareOp{};

    VkBool32 stencilTestEnable{};
    VkStencilOpState stencilFront{};
    VkStencilOpState stencilBack{};
  } depthStencil{};

  struct Primitive {
    VkPrimitiveTopology topology{};
    VkPolygonMode polygonMode{};
    VkCullModeFlags cullMode{};
    VkFrontFace frontFace{};
  } primitive{};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_PIPELINE_H
