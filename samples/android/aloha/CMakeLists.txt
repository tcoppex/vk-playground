# -----------------------------------------------------------------------------

set(SAMPLE_ROOT_PATH        ${CMAKE_CURRENT_SOURCE_DIR})
set(SAMPLE_SOURCE_DIR       ${SAMPLE_ROOT_PATH}/src/cpp)

get_filename_component(target ${SAMPLE_ROOT_PATH} NAME)

# -----------------------------------------------------------------------------

file(GLOB_RECURSE Sources
  ${SAMPLE_SOURCE_DIR}/*.cc
  ${SAMPLE_SOURCE_DIR}/*.h
)

add_library(${target} MODULE ${Sources})

# -----------------------------------------------------------------------------

# Here the sample's shaders will be compiled by gradle but we NEED to copy the
# framework shaders to the assets for the package to hold them.
#
# for now it is handle by gradle in a semi reliable way but we might
# have need to rethink it ulteriorly.

# compile_shaders(
#   ${SAMPLE_PATH}/shaders/glsl
#   ${SAMPLE_PATH}/shaders/spirv
#   shaders_bin
#   shaders_src
#   ${FRAMEWORK_SHADERS_DIR}
# )

add_dependencies(${target} BuildFrameworkShaders)

# -----------------------------------------------------------------------------

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
    COMPILED_SHADERS_DIR="shaders/"
    ${CustomDefinitions}
)

# -----------------------------------------------------------------------------
