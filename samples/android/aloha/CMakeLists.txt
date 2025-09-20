# -----------------------------------------------------------------------------
# 
#  aloha
# 
# -----------------------------------------------------------------------------

set(target aloha)

set(SAMPLE_ROOT_PATH               ${CMAKE_CURRENT_SOURCE_DIR})
set(SAMPLE_SOURCE_DIR              ${SAMPLE_ROOT_PATH}/src/cpp)

# -----------------------------------------------------------------------------

file(GLOB_RECURSE Sources
  ${SAMPLE_SOURCE_DIR}/*.cc
)

list(APPEND CustomIncludeDirs
  ${SAMPLE_SOURCE_DIR}
  ${WAFFLE_INCLUDE_DIRS}
)

list(APPEND CustomLibs
  ${WAFFLE_LIBRARIES}
  ${WAFFLE_FRAMEWORK_LIBRARIES}
)

list(APPEND CustomDefinitions
  ${WAFFLE_DEFINITIONS}
)

# -----------------------------------------------------------------------------

if(ANDROID)
  add_library(${target} MODULE ${Sources})
endif()

helpers_setupTarget(
  TARGET
    ${target}
  INCLUDE_DIRECTORIES
    ${CustomIncludeDirs}
  LIBRARIES
    ${CustomLibs}
  DEFINITIONS
    ${CustomDefinitions}
)

# -----------------------------------------------------------------------------
