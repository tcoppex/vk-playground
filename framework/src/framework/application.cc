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
    on_resize_cb_(w, h);
  }

  OnResizeCB on_resize_cb_{};
};

/* -------------------------------------------------------------------------- */

int Application::run(AppData_t app_data) {
  if (!presetup(app_data)) {
    return EXIT_FAILURE;
  }

  LOGD("--- App Setup ---");
  if (!setup()) {
    shutdown();
    return EXIT_FAILURE;
  }
  context_.allocator().clear_staging_buffers();

  LOGD("--- Mainloop ---");
  while (next_frame(app_data)) {
    auto const tick = elapsed_time();
    last_frame_time_ = frame_time_;
    frame_time_ = tick;

    if (!wm_->isActive()) {
      std::this_thread::sleep_for(10ms);
      continue;
    }

    ui_->beginFrame();
    build_ui();
    ui_->endFrame();

    update(delta_time());
    draw();
  }
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
  /* Singletons. */
  {
    Logger::Initialize();
    Events::Initialize();
  }

  LOGD("--- Framework Setup ---");

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

  /* Surface & Swapchain. */
  if (!reset_swapchain()) {
    LOGE("Surface creation fails");
    goto presetup_fails_surface;
  }

  /* Internal Renderer. */
  renderer_.init(context_, swapchain_, context_.allocator());

  /* User Interface. */
  if (ui_ = std::make_unique<UIController>(); !ui_ || !ui_->init(renderer_, *wm_)) {
    LOGE("UI creation fails");
    goto presetup_fails_ui;
  }

  /* Framework internal data. */
  {
    // ---------------------------------------------
    auto on_resize = [this](uint32_t w, uint32_t h) {
      context_.device_wait_idle();

      viewport_size_ = {
        .width = w,
        .height = h,
      };
      LOGV("> AppResize (w: {}, h: {})", viewport_size_.width, viewport_size_.height);

      reset_swapchain();

      // Signal the Renderer.
      renderer_.resize(viewport_size_.width, viewport_size_.height);
    };
    default_callbacks_ = std::make_unique<DefaultAppEventCallbacks>(on_resize);
    Events::Get().registerCallbacks(default_callbacks_.get());
    Events::Get().registerCallbacks(this);

    // LOGW("> Retrieve original viewport size (might be incorrect).");
    viewport_size_ = {
      .width = wm_->get_surface_width(),
      .height = wm_->get_surface_height(),
    };
    LOGD("> (w: {}, h: {})", viewport_size_.width, viewport_size_.height);
    // ---------------------------------------------

    // Time tracker.
    chrono_ = std::chrono::high_resolution_clock::now();

    // Initialize the standard C RNG seed, if any lib use it.
    rand_seed_ = static_cast<uint32_t>(std::time(nullptr));
    std::srand(rand_seed_);
  }

  LOGD("--------------------------------------------\n");

  return true;

  // [just for the hellish fun of it]
presetup_fails_ui:
  renderer_.deinit();
  swapchain_.deinit();
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

  return wm_->poll(app_data)
#if defined(ANDROID)
      && !app_data->destroyRequested
#endif
      ;
}

// ----------------------------------------------------------------------------

bool Application::reset_swapchain() {
  LOGD("[Reset Swapchain]");

  auto surface_creation = VK_SUCCESS;

  // Release previous swapchain if any, and create the surface when needed.
  if (VK_NULL_HANDLE == surface_) [[unlikely]] {
    surface_creation = CHECK_VK(
      wm_->createWindowSurface(context_.instance(), &surface_)
    );
  } else {
#if defined(ANDROID)
    context_.destroy_surface(surface_);
    swapchain_.deinit();
    surface_creation = CHECK_VK(
      wm_->createWindowSurface(context_.instance(), &surface_)
    );
#else
    swapchain_.deinit(true);
#endif
  }

  // Recreate the Swapchain.
  if (VK_SUCCESS == surface_creation) {
    swapchain_.init(context_, surface_);
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------

void Application::shutdown() {
  LOGD("--- Shutdown ---");
  context_.device_wait_idle();

  release();

  if (ui_) {
    ui_->release(context_);
  }

  renderer_.deinit();
  swapchain_.deinit();
  context_.destroy_surface(surface_);
  context_.deinit();
  wm_->shutdown();

  Events::Deinitialize();
  Logger::Deinitialize();
}

/* -------------------------------------------------------------------------- */
