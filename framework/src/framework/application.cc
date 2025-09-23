#include "framework/application.h"
#include "framework/core/platform/events.h"

// -----------------
#if defined(ANDROID)
#include "framework/core/platform/android/wm_android.h" //
#else
#include "framework/core/platform/desktop/window.h" //
#endif
// -----------------

/* -------------------------------------------------------------------------- */

int Application::run(AppData_t app_data) {
  if (!presetup(app_data) || !setup()) {
    return EXIT_FAILURE;
  }
  context_.allocator().clear_staging_buffers();

  while (next_frame(app_data)) {
    /* Clock */
    auto const tick = elapsed_time();
    last_frame_time_ = frame_time_;
    frame_time_ = tick;

    /* User Interface */
    ui_->beginFrame();
    build_ui();
    ui_->endFrame();

    /* User frame */
    frame();
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
    Events::Initialize();
  }

#if defined(ANDROID)
  wm_ = std::make_unique<WMAndroid>();
#else
  wm_ = std::make_unique<Window>();
#endif

  /* Create the main window surface. */
  if (!wm_ || !wm_->init(app_data)) {
    LOGE("window creation fails");
    goto presetup_fails_wm;
  }

  /* Initialize the Vulkan context. */
  if (!context_.init(wm_->getVulkanInstanceExtensions())) {
    LOGE("Vulkan context initialization fails");
    goto presetup_fails_context;
  }

  /* Create the context surface. */
  if (auto res = wm_->createWindowSurface(context_.instance(), &surface_); CHECK_VK(res)) {
    LOGE("Vulkan Surface creation fails");
    goto presetup_fails_surface;
  }

  // ----------------------------------

  /* Initialize the default renderer. */
  renderer_.init(context_, context_.allocator_ptr(), surface_);

  // ----------------------------------

  /* Initialize User Interface. */
  if (ui_ = std::make_unique<UIController>(); !ui_ || !ui_->init(renderer_, *wm_)) {
    LOGE("UI creation fails");
    goto presetup_fails_ui;
  }

  /* Generic App internal data */
  {
    Events::Get().registerCallbacks(this);

    viewport_size_ = {
      .width = wm_->get_surface_width(),
      .height = wm_->get_surface_height(),
    };

    // Init time tracker.
    chrono_ = std::chrono::high_resolution_clock::now();

    // Initialize the standard C RNG seed.
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
  CHECK_VK(vkDeviceWaitIdle(context_.device()));

  release();
  ui_->release(context_);
  renderer_.deinit();

  vkDestroySurfaceKHR(context_.instance(), surface_, nullptr);
  context_.deinit();
  wm_->shutdown();

  Events::Deinitialize();
}

/* -------------------------------------------------------------------------- */
