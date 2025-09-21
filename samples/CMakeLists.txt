# -----------------------------------------------------------------------------
#
# Samples CMakeFiles
#
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# External Assets Dependencies (CC0 or public domains).
# -----------------------------------------------------------------------------

# ---- Models ----

list(APPEND glbKhronosAssets
  DamagedHelmet     # on LFS
  AlphaBlendModeTest
)

foreach(asset IN LISTS glbKhronosAssets)
  set(GLB_KHRONOS_ASSETS_URL
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models"
  )
  download_model(
    "${GLB_KHRONOS_ASSETS_URL}/${asset}/glTF-Binary/${asset}.glb"
  )
endforeach()

download_model(
  "https://raw.githubusercontent.com/SaschaWillems/VulkanPathTracer/refs/heads/master/data/models/CornellBox-Original.gltf"
)

# ---- Textures ----

list(APPEND hdriPolyhavenAssets
  qwantani_dusk_2_2k    # on LFS
  rogland_clear_night_2k
)

foreach(asset IN LISTS hdriPolyhavenAssets)
  set(HDRI_POLYHAVEN_ASSETS_URL
    "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/2k"
  )
  download_texture(
    "${HDRI_POLYHAVEN_ASSETS_URL}/${asset}.hdr"
  )
endforeach()

# -----------------------------------------------------------------------------

function(add_simple_demo dirname)
  set(SAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${dirname})

  file(GLOB Source
    ${SAMPLE_PATH}/*.cc
    ${SAMPLE_PATH}/*.h
  )

  compile_shaders(
    ${SAMPLE_PATH}/shaders/glsl
    ${SAMPLE_PATH}/shaders/spirv
    shaders_bin
    shaders_src
    ${FRAMEWORK_SHADERS_DIR}
  )

  set(BuildShaderTarget BuildSampleShaders_${dirname})
  add_custom_target(
    ${BuildShaderTarget} ALL
    DEPENDS
      ${shaders_bin}
      ${shaders_src}
  )

  if(ANDROID)
    add_library(${dirname} MODULE ${Source})
  else()
    add_executable(${dirname} ${Source})
  endif()
  add_dependencies(${dirname} ${BuildShaderTarget})

  helpers_setupTarget(
    TARGET
      ${dirname}

    INCLUDE_DIRECTORIES
      ${SAMPLE_PATH}
      ${SharedIncludeDirs}

    LIBRARIES
      ${FRAMEWORK_LIBRARIES}
      ${SharedLibs}

    DEFINITIONS
      COMPILED_SHADERS_DIR="${SAMPLE_PATH}/shaders/spirv/"
      ${CustomDefinitions}
  )

  set_target_output_directory(${dirname} ${PROJECT_BINARY_DIR})
endfunction(add_simple_demo)

# -----------------------------------------------------------------------------

if(ANDROID)
add_subdirectory(${SAMPLES_PATH}/android)
else()
add_subdirectory(${SAMPLES_PATH}/desktop)
endif()

# -----------------------------------------------------------------------------
