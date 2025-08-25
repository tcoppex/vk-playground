/* -------------------------------------------------------------------------- */
//
//    10 - material
//
//  Where we don't bother and use the internal material & rendering system.
//
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"

#include "framework/scene/camera.h"
#include "framework/scene/arcball_controller.h"

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 private:
  bool setup() final {
    wm_->setTitle("10 - materia gonanza");

    renderer_.set_color_clear_value({ 0.72f, 0.28f, 0.30f, 0.0f });
    renderer_.skybox().setup(ASSETS_DIR "textures/"
      // "qwantani_dusk_2_2k.hdr"
      "rogland_clear_night_2k.hdr"
    );

    /* Setup the ArcBall camera. */
    {
      camera_.setPerspective(
        lina::radians(55.0f),
        viewport_size_.width,
        viewport_size_.height,
        0.01f,
        500.0f
      );
      camera_.setController(&arcball_controller_);

      arcball_controller_.setView(lina::kTwoPi/16.0f, lina::kTwoPi/8.0f, false);
      arcball_controller_.setDolly(4.0f, false);
    }

    /* Load a glTF Scene. */
    future_scene_ = renderer_.async_load_to_device(ASSETS_DIR "models/"
      // "DamagedHelmet.glb"
      "simple_kaidou.glb"
      // "boulder_01_4k.glb"
    );

    return true;
  }

  void build_ui() final {
    ImGui::Begin("Settings");
    {
      ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
      ImGui::Separator();
    }
    ImGui::End();
  }

  void release() final {
    if (scene_) {
      scene_->release();
    }
  }

  void update(float const dt) final {
    camera_.update(dt);

    if (future_scene_.valid()
     && future_scene_.wait_for(0ms) == std::future_status::ready) {
      scene_ = future_scene_.get();
    }
  }

  void draw() final {
    auto cmd = renderer_.begin_frame();
    {
      //---------------------
      auto pass = cmd.begin_rendering();

      if (auto const& skybox = renderer_.skybox(); skybox.is_valid()) {
        skybox.render(pass, camera_);
      }

      if (scene_) {
        scene_->render(pass, camera_);
      }
      cmd.end_rendering();
      //---------------------

      cmd.render_ui(renderer_);
    }
    renderer_.end_frame();
  }

 private:
  Camera camera_{};
  ArcBallController arcball_controller_{};

  std::future<GLTFScene> future_scene_{};
  GLTFScene scene_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
