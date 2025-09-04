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

#include "framework/renderer/fx/postprocess/ray_tracing/ray_tracing_fx.h"

/* -------------------------------------------------------------------------- */

class BasicRayTracingFx : public RayTracingFx {
 protected:
  backend::ShadersMap createShaderModules() const final {
    auto create_modules{[&](std::vector<std::string_view> const& filenames) {
      return context_ptr_->create_shader_modules(
        COMPILED_SHADERS_DIR,
        filenames
      );
    }};
    return {
      {
        backend::ShaderStage::Raygen,
        create_modules({ "raygen.rgen" })
      },
      {
        backend::ShaderStage::ClosestHit,
        create_modules({ "closesthit.rchit" })
      },
      {
        backend::ShaderStage::Miss,
        create_modules({ "miss.rmiss" })
      },
    };
  }

  RayTracingPipelineDescriptor_t pipelineDescriptor(backend::ShadersMap const& shaders_map) final {
    auto shader_index{[&](std::string_view shader_name) -> uint32_t {
      uint32_t index = 0;
      for (auto const& [stage, shaders] : shaders_map) {
        for (auto const& shader : shaders) {
          if (shader.basename == shader_name) {
            return index;
          }
          ++index;
        }
      }
      return kInvalidIndexU32;
    }};

    return {
      .shaders = {
        .raygens      = shaders_map.at(backend::ShaderStage::Raygen),
        .closestHits  = shaders_map.at(backend::ShaderStage::ClosestHit),
        .misses       = shaders_map.at(backend::ShaderStage::Miss),
      },

      // ~ ORDER MATTERS ... ~
      .shaderGroups = {
        // Raygen Groups
        .raygens = {{
          .type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
          .generalShader = shader_index("raygen.rgen"),
        }},

        // Miss Groups
        .misses = {{
          .type           = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
          .generalShader  = shader_index("miss.rmiss"),
        }},

        // Hit Groups
        .hits = {{
          .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
          .closestHitShader   = shader_index("closesthit.rchit"),
          .anyHitShader       = VK_SHADER_UNUSED_KHR,
          .intersectionShader = VK_SHADER_UNUSED_KHR, // only on PROCEDURAL type
        }},
      }
    };
  }
};

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 private:
  bool setup() final {
    wm_->setTitle("11 - shining through");

    renderer_.set_color_clear_value({ 0.72f, 0.28f, 0.30f, 0.0f });

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
    raytracing_.init(renderer_);
    raytracing_.setup(renderer_.get_surface_size());
    // -------------------------------

    /* Load a glTF Scene. */
    std::string gtlf_filename{ASSETS_DIR "models/"
      // "sponza.glb"
      "DamagedHelmet.glb"
      // "Duck.glb"
    };

    if constexpr(true) {
      future_scene_ = renderer_.async_load_to_device(gtlf_filename);
    } else {
      scene_ = renderer_.load_and_upload(gtlf_filename);
    }

    return true;
  }

  void release() final {
    raytracing_.release();
    scene_.reset();
  }

  void update(float const dt) final {
    camera_.update(dt);

    if (future_scene_.valid()
     && future_scene_.wait_for(0ms) == std::future_status::ready) {
      scene_ = future_scene_.get();
      // -------------------------------
      scene_->set_ray_tracing_fx(&raytracing_);
      // -------------------------------
    }
    if (scene_) {
      scene_->update(camera_, renderer_.get_surface_size(), elapsed_time());
    }
  }

  void draw() final {
    auto cmd = renderer_.begin_frame();

    if constexpr(true)
    {
      // RAY TRACER
      raytracing_.execute(cmd);
      cmd.blit(raytracing_, renderer_);
    }
    else
    {
      // RASTERIZER
      auto pass = cmd.begin_rendering();
      if (scene_) scene_->render(pass);
      cmd.end_rendering();
    }

    renderer_.end_frame();
  }

 private:
  Camera camera_{};
  ArcBallController arcball_controller_{};

  BasicRayTracingFx raytracing_{};

  std::future<GLTFScene> future_scene_{};
  GLTFScene scene_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
