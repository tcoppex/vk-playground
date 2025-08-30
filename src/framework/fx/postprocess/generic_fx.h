#ifndef VKPLAYGROUND_FRAMEWORK_RENDERER_GENERIC_FX_H_
#define VKPLAYGROUND_FRAMEWORK_RENDERER_GENERIC_FX_H_

#include "framework/fx/postprocess/fx_interface.h"

/* -------------------------------------------------------------------------- */

class GenericFx : public virtual FxInterface {
 public:
  virtual ~GenericFx() {}

  void init(Context const& context, Renderer const& renderer) override;

  void setup(VkExtent2D const dimension) override;

  void release() override;

  void setupUI() override {}

  std::string name() const override;

 protected:
  virtual std::string getShaderName() const = 0;

  virtual std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const = 0;

  virtual std::vector<VkPushConstantRange> getPushConstantRanges() const {
    return {};
  }

  virtual void pushConstant(GenericCommandEncoder const& cmd) {}

  virtual void createPipelineLayout();

  virtual void createPipeline() = 0;

 protected:
  std::shared_ptr<ResourceAllocator> allocator_{};

  // ----------------
  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{}; //
  VkPipelineLayout pipeline_layout_{}; // (redundant, as also kept in pipeline_ when created)
  // ----------------

  Pipeline pipeline_{};
};

/* -------------------------------------------------------------------------- */

class PostGenericFx : public virtual GenericFx
                    , public PostFxInterface {
 public:
  void setup(VkExtent2D const dimension) override {
    resize(dimension);
    GenericFx::setup(dimension);
  }

 public:
  bool isEnabled() const {
    return enabled_;
  }

  bool enabled_{true};
};

/* -------------------------------------------------------------------------- */

#endif // VKPLAYGROUND_FRAMEWORK_RENDERER_GENERIC_FX_H_
