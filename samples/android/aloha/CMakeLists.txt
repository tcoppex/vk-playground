# -----------------------------------------------------------------------------

set(SAMPLE_ROOT_PATH        ${CMAKE_CURRENT_SOURCE_DIR})
set(SAMPLE_SOURCE_DIR       ${SAMPLE_ROOT_PATH}/src/cpp)

get_filename_component(target ${SAMPLE_ROOT_PATH} NAME)

# -----------------------------------------------------------------------------

file(GLOB_RECURSE Sources
  ${SAMPLE_SOURCE_DIR}/*.cc
  ${SAMPLE_SOURCE_DIR}/*.h
)

# compile_shaders(
#   ${SAMPLE_PATH}/shaders/glsl
#   ${SAMPLE_PATH}/shaders/spirv
#   shaders_bin
#   shaders_src
#   ${FRAMEWORK_SHADERS_DIR}
# )

# -----------------------------------------------------------------------------

add_library(${target} MODULE ${Sources})

helpers_setupTarget(
  TARGET
    ${target}

  INCLUDE_DIRECTORIES
    ${SAMPLE_SOURCE_DIR}
    ${SharedIncludeDirs}

  LIBRARIES
    ${FRAMEWORK_LIBRARIES}
    ${SharedLibs}

  DEFINITIONS
    COMPILED_SHADERS_DIR="${SAMPLE_PATH}/shaders/spirv/" #!!!!
    ${CustomDefinitions}
)

# -----------------------------------------------------------------------------
