# -----------------------------------------------------------------------------
#
# Samples
#
# -----------------------------------------------------------------------------

## Setup a Sample target property.
## (currently only used by desktop samples, Android use a custom one)
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

add_subdirectory(${SAMPLES_PATH}/android)
add_subdirectory(${SAMPLES_PATH}/desktop)

# -----------------------------------------------------------------------------
