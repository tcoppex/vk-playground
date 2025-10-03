/* -------------------------------------------------------------------------- */
//
//    02 - Aloha XR
//
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
private:
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
  std::vector<char const*> vulkanDeviceExtensions() const noexcept final {
    return {
      // VK_KHR_MULTIVIEW_EXTENSION_NAME, //
    };
  }

  bool setup() final {
    renderer_.set_color_clear_value(vec4(0.75f, 0.15f, 0.30f, 1.0f));
    return true;
  }

  void draw() final {
    auto cmd = renderer_.begin_frame();

    // using VK_KHR_multiview might need some adjustement inside 'begin_rendering'
    // for the VkRenderingInfo layout number and viewmask
    //
    //  Also, view specific shaders would need to be reworked (cameraUbo x 2).

    auto pass = cmd.begin_rendering();
    {
      //
    }
    cmd.end_rendering();

    renderer_.end_frame();
  }

  void onPointerUp(int x, int y, KeyCode_t button) final {
    LOGD("{}", __FUNCTION__);
    clear_color_index_ = (clear_color_index_ + 1u) % static_cast<uint32_t>(kColors.size());
    renderer_.set_color_clear_value(kColors[clear_color_index_]);
  }

 private:
  uint32_t clear_color_index_{};
};

// ----------------------------------------------------------------------------

ENTRY_POINT(SampleApp)

/* -------------------------------------------------------------------------- */
