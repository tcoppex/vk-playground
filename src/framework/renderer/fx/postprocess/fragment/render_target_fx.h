#ifndef VKFRAMEWORK_RENDERER_POST_FX_RENDER_TARGET_H
#define VKFRAMEWORK_RENDERER_POST_FX_RENDER_TARGET_H

#include "framework/renderer/fx/postprocess/fragment/fragment_fx.h"
#include "framework/renderer/fx/postprocess/post_generic_fx.h"
#include "framework/renderer/legacy/render_target.h"

/* -------------------------------------------------------------------------- */

class RenderTargetFx : public FragmentFx
                     , public PostGenericFx {
 public:
  static std::string GetMapScreenVertexShaderName();

 public:
  void release() override;

  void execute(CommandEncoder& cmd) override; //

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

  GraphicsPipelineDescriptor_t getGraphicsPipelineDescriptor(std::vector<backend::ShaderModule> const& shaders) const override {
     return {
      .vertex = {
        .module = shaders[0u].module,
      },
      .fragment = {
        .module = shaders[1u].module,
        .targets = {
          { .format = render_target_->get_color_attachment().format },
        }
      },
      .primitive = {
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .cullMode = VK_CULL_MODE_BACK_BIT,
      },
    };
  }

  VkExtent2D getRenderSurfaceSize() const override {
    return render_target_->get_surface_size();
  }

  void draw(RenderPassEncoder const& pass) override {
    pass.draw(3u);
  }

  virtual void createRenderTarget(VkExtent2D const dimension);

 protected:
  std::shared_ptr<RenderTarget> render_target_{};
  std::vector<backend::Buffer> unused_buffers_{};
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_POST_FX_RENDER_TARGET_H
