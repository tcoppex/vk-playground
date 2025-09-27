# -----------------------------------------------------------------------------
#
# Desktop Samples
#
# -----------------------------------------------------------------------------

## Setup a Sample target property.
function(add_simple_demo dirname)
  set(SAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${dirname})
  set(target ${dirname})

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

  set(BuildShaderTarget BuildSampleShaders_${target})
  add_custom_target(
    ${BuildShaderTarget} ALL
    DEPENDS
      ${shaders_bin}
      ${shaders_src}
  )

  # if(ANDROID)
  #   add_library(${target} MODULE ${Source})
  # else()
  #   add_executable(${target} ${Source})
  # endif()
  add_executable(${target} ${Source})

  add_dependencies(${target} ${BuildShaderTarget})

  helpers_setupTarget(
    TARGET
      ${target}

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

add_simple_demo(00_hello)
add_simple_demo(01_triangle)
add_simple_demo(02_push_constant)
add_simple_demo(03_descriptor_set)
add_simple_demo(04_texturing)
add_simple_demo(05_stencil_op)
add_simple_demo(06_blend_op)
add_simple_demo(07_compute)
add_simple_demo(08_hdr_envmap)
add_simple_demo(09_post_process)
add_simple_demo(10_material)
add_simple_demo(11_raytracing)

# -----------------------------------------------------------------------------