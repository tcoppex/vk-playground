#include "framework/application.h"
#include "framework/core/platform/events.h"
#include "framework/core/platform/window.h"

/* -------------------------------------------------------------------------- */

struct DefaultAppEventCallbacks final : public EventCallbacks {
  using OnResizeCB = std::function<void(int32_t, int32_t)>;

  DefaultAppEventCallbacks(OnResizeCB on_resize_cb)
    : on_resize_cb_(on_resize_cb)
  {}
  ~DefaultAppEventCallbacks() = default;

  void onResize(int w, int h) final {
    LOGI("DefaultAppEventCallbacks::%s", __FUNCTION__);
    on_resize_cb_(w, h);
  }

  OnResizeCB on_resize_cb_;
};

/* -------------------------------------------------------------------------- */

int Application::run(AppData_t app_data) {
  /* Singletons. */
  {
    Logger::Initialize();
    Events::Initialize();
  }

  LOGD("--- Framework Initialization ---");
  if (!presetup(app_data)) {
    return EXIT_FAILURE;
  }
  LOGD("--------------------------------------------\n");

  LOGD("--- App Initialization ---");
  if (!setup()) {
    return EXIT_FAILURE;
  }
  context_.allocator().clear_staging_buffers();
  LOGD("--------------------------------------------\n");

  LOGD("--- Mainloop ---");
  while (next_frame(app_data)) {
    auto const tick = elapsed_time();
    last_frame_time_ = frame_time_;
    frame_time_ = tick;

    ui_->beginFrame();
    build_ui();
    ui_->endFrame();

    frame();
  }

  LOGD("--- Shutdown ---");
  shutdown();

  return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------------

float Application::elapsed_time() const noexcept {
  auto now{ std::chrono::high_resolution_clock::now() };
  return std::chrono::duration<float>(now - chrono_).count();
}

// ----------------------------------------------------------------------------

bool Application::presetup(AppData_t app_data) {
  /* Window manager. */
  if (wm_ = std::make_unique<Window>(); !wm_ || !wm_->init(app_data)) {
    LOGE("Window creation fails");
    goto presetup_fails_wm;
  }

  /* Vulkan context. */
  if (!context_.init(wm_->getVulkanInstanceExtensions())) {
    LOGE("Vulkan context initialization fails");
    goto presetup_fails_context;
  }

  /* Platform rendering surface. */
  if (auto res = wm_->createWindowSurface(context_.instance(), &surface_); CHECK_VK(res) != VK_SUCCESS) {
    LOGE("Vulkan Surface creation fails");
    goto presetup_fails_surface;
  }

  /* Swapchain. */
  swapchain_.init(context_, surface_);

  // ----------------------------------

  /* Internal Renderer. */
  renderer_.init(context_, swapchain_, context_.allocator());

  // ----------------------------------

  /* User Interface. */
  if (ui_ = std::make_unique<UIController>(); !ui_ || !ui_->init(renderer_, *wm_)) {
    LOGE("UI creation fails");
    goto presetup_fails_ui;
  }

  /* Framework internal data. */
  {
    // ---------------------------------------------
    // ---------------------------------------------
    auto onResize = [this](int w, int h) {
      context_.device_wait_idle();

      LOGW("> AppResize old(w: %u, h: %u).", viewport_size_.width, viewport_size_.height);
      viewport_size_ = {
        .width = (uint32_t)w, //wm_->get_surface_width(),
        .height = (uint32_t)h, //wm_->get_surface_height(),
      };
      LOGW("> AppResize new(w: %u, h: %u).", viewport_size_.width, viewport_size_.height);

      LOGD("destroy swapchain");
      swapchain_.deinit();

      // Recreate the surface.
      LOGD("destroy surface");
      vkDestroySurfaceKHR(context_.instance(), surface_, nullptr);

      LOGD("create surface");
      if (VK_SUCCESS == CHECK_VK(wm_->createWindowSurface(context_.instance(), &surface_))) {
        // Recreate the Swapchain.
        swapchain_.init(context_, surface_);

        // Signal the Renderer.
        renderer_.resize(viewport_size_.width, viewport_size_.height);
      }
    };

    default_callbacks_ = std::make_unique<DefaultAppEventCallbacks>(onResize);
    Events::Get().registerCallbacks(default_callbacks_.get());
    Events::Get().registerCallbacks(this);

    LOGW("> Retrieve original viewport size (might be incorrect).");
    viewport_size_ = {
      .width = wm_->get_surface_width(),
      .height = wm_->get_surface_height(),
    };
    LOGW("> (w: %u, h: %u).", viewport_size_.width, viewport_size_.height);

    // onResize(wm_->get_surface_width(), wm_->get_surface_height());
    // renderer_.resize(viewport_size_.width, viewport_size_.height);

    // ---------------------------------------------
    // ---------------------------------------------


    // Time tracker.
    chrono_ = std::chrono::high_resolution_clock::now();

    // Initialize the standard C RNG seed, if any lib use it.
    rand_seed_ = static_cast<uint32_t>(std::time(nullptr));
    std::srand(rand_seed_);
  }

  return true;

  // [just for the hellish fun of it]
presetup_fails_ui:
  renderer_.deinit();
presetup_fails_surface:
  context_.deinit();
presetup_fails_context:
  wm_->shutdown();
presetup_fails_wm:
  return false;
}

// ----------------------------------------------------------------------------

bool Application::next_frame(AppData_t app_data) {
  Events::Get().prepareNextFrame();

  return true
#if defined(ANDROID)
      && !app_data->destroyRequested
#endif
      && wm_->poll(app_data)
  ;
}

// ----------------------------------------------------------------------------

void Application::shutdown() {
  context_.device_wait_idle();

  release();
  ui_->release(context_);
  renderer_.deinit();

  swapchain_.deinit();
  vkDestroySurfaceKHR(context_.instance(), surface_, nullptr);

  context_.deinit();
  wm_->shutdown();

  Events::Deinitialize();
  Logger::Deinitialize();
}

/* -------------------------------------------------------------------------- */
