#ifndef VKFRAMEWORK_RENDERER_FX_POSTPROCESS_GENERIC_FX_H_
#define VKFRAMEWORK_RENDERER_FX_POSTPROCESS_GENERIC_FX_H_

#include "framework/renderer/fx/postprocess/fx_interface.h"

/* -------------------------------------------------------------------------- */

class GenericFx : public virtual FxInterface {
 public:
  virtual ~GenericFx() {
    release();
  }

  void init(Renderer const& renderer) override;

  void setup(VkExtent2D const dimension) override;

  void release() override;

  void setupUI() override {}

  std::string name() const override; //

 protected:
  // [deprecated]
  virtual std::string getShaderName() const = 0; //
  // virtual backend::ShadersMap createShaderModules() const = 0;

  virtual std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const = 0;

  virtual std::vector<VkPushConstantRange> getPushConstantRanges() const {
    return {};
  }

  virtual void pushConstant(GenericCommandEncoder const& cmd) {}

  virtual void createPipelineLayout();

  virtual void createPipeline() = 0;

 protected:
  ResourceAllocator const* allocator_ptr_{};

  // ----------------
  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{}; //
  VkPipelineLayout pipeline_layout_{}; // (redundant, as also kept in pipeline_ when created)
  // ----------------

  Pipeline pipeline_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_POSTPROCESS_GENERIC_FX_H_
