/* -------------------------------------------------------------------------- */
//
//    11 - raytracing
//
//
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/core/camera.h"
#include "framework/core/arcball_controller.h"

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 private:
  bool setup() final {
    wm_->setTitle("11 - shining through");

    renderer_.set_color_clear_value({ 0.72f, 0.28f, 0.30f, 0.0f });
    // renderer_.skybox().setup(ASSETS_DIR "textures/"
    //   "rogland_clear_night_2k.hdr"
    // );

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

    // -------------------------------
    auto shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "raygen.rgen",
      "closesthit.rchit",
      "miss.rmiss",
    })};

    pipeline_layout_  = renderer_.create_pipeline_layout({});

    pipeline_ = renderer_.create_raytracing_pipeline(pipeline_layout_, {
      .raygens = { {.module = shaders[0].module} },
      .closestHits = { {.module = shaders[1].module} },
      .misses = { {.module = shaders[2].module} },
      .shaderGroups = {
        // Raygen Group
        {
          .type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
          .generalShader    = 0,
        },
        // Hit Group
        {
          .type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
          .closestHitShader = 1,
        },
        // Miss Group
        {
          .type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
          .generalShader    = 2,
        },
      }
    });

    context_.release_shader_modules(shaders);

    // -------------------------------

    /* Load a glTF Scene. */
    std::string gtlf_filename{ASSETS_DIR "models/"
      "sponza.glb"
    };

    if constexpr(true) {
      future_scene_ = renderer_.async_load_to_device(gtlf_filename);
    } else {
      scene_ = renderer_.load_and_upload(gtlf_filename);
    }

    return true;
  }

  void release() final {
    scene_.reset();
    renderer_.destroy_pipeline(pipeline_);
    renderer_.destroy_pipeline_layout(pipeline_layout_);
  }

  void update(float const dt) final {
    camera_.update(dt);

    if (future_scene_.valid()
     && future_scene_.wait_for(0ms) == std::future_status::ready) {
      scene_ = future_scene_.get();
    }
    if (scene_) {
      scene_->update(camera_, renderer_.get_surface_size(), elapsed_time());
    }
  }

  void draw() final {
    auto cmd = renderer_.begin_frame();
    {
      auto pass = cmd.begin_rendering();
      {
        // SKYBOX.
        if (auto const& skybox = renderer_.skybox(); skybox.is_valid()) {
          skybox.render(pass, camera_);
        }

        // SCENE.
        if (scene_) {
          scene_->render(pass);
        }
      }
      cmd.end_rendering();

      // UI.
      cmd.render_ui(renderer_);
    }
    renderer_.end_frame();
  }

 private:
  Camera camera_{};
  ArcBallController arcball_controller_{};

  VkPipelineLayout pipeline_layout_{};
  Pipeline pipeline_{};

  std::future<GLTFScene> future_scene_{};
  GLTFScene scene_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
