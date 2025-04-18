#ifndef HELLOVK_FRAMEWORK_RENDERER_GENERIC_FX_H_
#define HELLOVK_FRAMEWORK_RENDERER_GENERIC_FX_H_

#include "framework/common.h"
#include "framework/renderer/renderer.h"
class CommandEncoder;

#include "framework/fx/_experimental/fx_interface.h"

/* -------------------------------------------------------------------------- */

class GenericFx : public FxInterface {
 public:
  bool isEnabled() const {
    return enabled_;
  }

 public:
  void init(Context const& context, Renderer const& renderer) override {
    FxInterface::init(context, renderer);
    allocator_ = context.get_resource_allocator();
  }

  void setup(VkExtent2D const dimension) override {
    assert(nullptr != context_ptr_);

    resize(dimension);
    createPipelineLayout();
    createPipeline();
  }

  void release() override {
    if (pipeline_layout_ == VK_NULL_HANDLE) {
      return;
    }
    renderer_ptr_->destroy_pipeline(pipeline_);
    renderer_ptr_->destroy_pipeline_layout(pipeline_layout_);
    renderer_ptr_->destroy_descriptor_set_layout(descriptor_set_layout_);
    pipeline_layout_ = VK_NULL_HANDLE;
  }

  void setupUI() override {
  }

  std::string name() const override {
    std::filesystem::path fn(getShaderName());
    return fn.stem().string();
  }

 protected:
  virtual std::string getShaderName() const = 0;

  virtual std::vector<DescriptorSetLayoutParams> getDescriptorSetLayoutParams() const = 0;

  virtual std::vector<VkPushConstantRange> getPushConstantRanges() const {
    return {};
  }

  virtual void updatePushConstant(GenericCommandEncoder const& cmd) {
  }

  virtual void createPipelineLayout() {
    descriptor_set_layout_ = renderer_ptr_->create_descriptor_set_layout(getDescriptorSetLayoutParams());
    descriptor_set_ = renderer_ptr_->create_descriptor_set(descriptor_set_layout_);
    pipeline_layout_ = renderer_ptr_->create_pipeline_layout({
      .setLayouts = { descriptor_set_layout_ },
      .pushConstantRanges = getPushConstantRanges()
    });
  }

  virtual void createPipeline() = 0;

 protected:
  std::shared_ptr<ResourceAllocator> allocator_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  VkPipelineLayout pipeline_layout_{};

  Pipeline pipeline_{}; //

  bool enabled_{true};
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_RENDERER_GENERIC_FX_H_
