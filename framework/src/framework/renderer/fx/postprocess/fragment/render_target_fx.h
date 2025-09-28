#ifndef VKFRAMEWORK_RENDERER_FX_POSTPROCESS_FRAGMENT_RENDER_TARGET_FX_H_
#define VKFRAMEWORK_RENDERER_FX_POSTPROCESS_FRAGMENT_RENDER_TARGET_FX_H_

#include "framework/renderer/fx/postprocess/fragment/fragment_fx.h"
#include "framework/renderer/fx/postprocess/post_generic_fx.h"
#include "framework/renderer/targets/render_target.h"

/* -------------------------------------------------------------------------- */

class RenderTargetFx : public FragmentFx
                     , public PostGenericFx {
 public:
  static std::string GetMapScreenVertexShaderName();

 public:
  void release() override;

  void execute(CommandEncoder& cmd) const override; //

 public:
  bool resize(VkExtent2D const dimension) override;

  backend::Image getImageOutput(uint32_t index = 0u) const override;

  virtual std::vector<backend::Image> const& getImageOutputs() const override;

  backend::Buffer getBufferOutput(uint32_t index = 0u) const override {
    return unused_buffers_[index];
  }

  std::vector<backend::Buffer> const& getBufferOutputs() const override {
    return unused_buffers_;
  }

 protected:
  std::string getVertexShaderName() const override {
    return GetMapScreenVertexShaderName();
  }

  GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(
    std::vector<backend::ShaderModule> const& shaders
  ) const override;

  VkExtent2D getRenderSurfaceSize() const override;

  void draw(RenderPassEncoder const& pass) const override {
    pass.draw(3u);
  }

  virtual void createRenderTarget(VkExtent2D const dimension);

 protected:
  std::shared_ptr<RenderTarget> render_target_{}; //
  std::vector<backend::Buffer> unused_buffers_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_FX_POSTPROCESS_FRAGMENT_RENDER_TARGET_FX_H_
