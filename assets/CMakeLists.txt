# -----------------------------------------------------------------------------
#
# External Assets Dependencies (CC0 or public domains).
#
# -----------------------------------------------------------------------------

# ---- Models -----------------------------------------------------------------

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

# ---- Textures ---------------------------------------------------------------

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
