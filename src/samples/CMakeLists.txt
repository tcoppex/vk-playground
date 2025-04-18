# -----------------------------------------------------------------------------

function(add_simple_demo dirname)
  set(SAMPLE_PATH ${SAMPLES_PATH}/${dirname})

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

  add_executable(${dirname} ${Source} ${shaders_bin} ${shaders_src})

  target_compile_options(${dirname} PRIVATE
    "${CXX_FLAGS}"
    "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
    "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>"
    "$<$<CONFIG:DebugWithRelInfo>:${CXX_FLAGS_RELWITHDEBINFO}>"
  )

  set_target_properties(${dirname}
    PROPERTIES
      LINK_FLAGS "${CXX_LINK_FLAGS}"
  )

  target_link_libraries(${dirname} PRIVATE
    ${FRAMEWORK_LIBRARIES}
    ${SharedLibs}
  )

  target_include_directories(${dirname} PRIVATE
    ${SAMPLE_PATH}
    ${SharedIncludeDirs}
  )

  target_compile_definitions(${dirname} PRIVATE
    COMPILED_SHADERS_DIR="${SAMPLE_PATH}/shaders/spirv/"
    FRAMEWORK_COMPILED_SHADERS_DIR="${FRAMEWORK_COMPILED_SHADERS_DIR}/" #!

    ASSETS_DIR="${PROJECT_ASSETS_DIR}/"

    # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
    # This is necessary because we are using volk to load Vulkan functions
    VK_NO_PROTOTYPES
    _USE_MATH_DEFINES
  )

  set_target_output_directory(${dirname} ${PROJECT_BINARY_DIR})
endfunction(add_simple_demo)

# -----------------------------------------------------------------------------

add_simple_demo(00_hello)
add_simple_demo(01_triangle)
add_simple_demo(02_push_constant)
add_simple_demo(03_descriptor_set)
add_simple_demo(04_texturing)
add_simple_demo(05_stencil_op)
add_simple_demo(06_blend_op)
add_simple_demo(07_compute)
add_simple_demo(08_hdr_envmap)

# add_simple_demo(999_test)

# -----------------------------------------------------------------------------
