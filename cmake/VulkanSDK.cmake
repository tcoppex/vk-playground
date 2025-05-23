# 
# CMake helpers commands to find and build GLSL file directly to spirv,
# with include handling and shader stage detection (based on prefix / suffix).
# 
# this need the Vulkan SDK installed.
# 

# -----------------------------------------------------------------------------

## Search the GLSL Compiler binary
if (WIN32)
  if (CMAKE_CL_64)
    find_program(GLSLC glslc
      "$ENV{VULKAN_SDK}/Bin"
      "$ENV{VK_SDK_PATH}/Bin"
    )
  else()
    find_program(GLSLC glslc
      "$ENV{VULKAN_SDK}/Bin32"
      "$ENV{VK_SDK_PATH}/Bin32"
    )
  endif()
else()
    find_program(GLSLC glslc
      "$ENV{VULKAN_SDK}/bin"
    )
endif()

# -----------------------------------------------------------------------------

## Custom function to generate binary shaders from GLSL, with include handling.
function(glsl2spirv input_glsl output_spirv shader_dir deps extra_args)
  # Retrieve the input file name
  get_filename_component(fn ${input_glsl} NAME)
  
  # Detects shader type based on its suffix or prefix
  if (${fn} MATCHES "((vert|vs)_.+\\.glsl)|(.+\\.(vert|vs)(\\.glsl)?)")
    set(stage "vert")
  elseif(${fn} MATCHES "((tesc|tcs)_.+\\.glsl)|(.+\\.(tesc|tcs)(\\.glsl)?)")
    set(stage "tesc")
  elseif(${fn} MATCHES "((tese|tes)_.+\\.glsl)|(.+\\.(tese|tes)(\\.glsl)?)")
    set(stage "tese")
  elseif(${fn} MATCHES "((geom|gs)_.+\\.glsl)|(.+\\.(geom|gs)(\\.glsl)?)")
    set(stage "geom")
  elseif(${fn} MATCHES "((frag|fs)_.+\\.glsl)|(.+\\.(frag|fs)(\\.glsl)?)")
    set(stage "frag")
  elseif(${fn} MATCHES "((comp|cs)_.+\\.glsl)|(.+\\.(comp|cs)(\\.glsl)?)")
    set(stage "comp")
  elseif(${fn} MATCHES "((mesh|ms)_.+\\.glsl)|(.+\\.(mesh|ms)(\\.glsl)?)")
    set(stage "mesh")
  else()
    message(WARNING "Unknown shader type for ${fn}")
    return()
  endif()

  get_filename_component(output_dir ${output_spirv} DIRECTORY)
  file(MAKE_DIRECTORY ${output_dir})

  # Compile to SPIR-V with include directory set to shaderdir
  add_custom_command(
    OUTPUT
      ${output_spirv}
    COMMAND
      ${GLSLC} -fshader-stage=${stage} -o ${output_spirv} ${input_glsl} -I ${shader_dir} ${extra_args}
    DEPENDS
      ${input_glsl}
      ${GLSLC}
      ${deps}
    WORKING_DIRECTORY
      ${CMAKE_SOURCE_DIR}
    COMMENT
      "Converting shader ${input_glsl} to ${output_spirv}" VERBATIM
    SOURCES
      ${input_glsl}
  )

# Forcing compilation can be tricky.
# To always recompile, use 
# add_custom_target( gen_${fn} ALL ..
# otherwise, set the output files as dependencies.
endfunction(glsl2spirv)

# -----------------------------------------------------------------------------

# Compile all shader from one directory to another
function(compile_shaders GLOBAL_GLSL_DIR GLOBAL_SPIRV_DIR binaries sources extra_dir)
  # retrieve all SOURCE glsl shaders
  file(GLOB_RECURSE g_ShadersGLSL ${GLOBAL_GLSL_DIR}/*.glsl)

  # Only keep shaders of the form "filename.stage.glsl"
  list(
    FILTER
      g_ShadersGLSL
    INCLUDE
    REGEX
    ".+\\..+\\.glsl$"
  )

  file(GLOB ShadersDependencies
    ${GLOBAL_GLSL_DIR}/../*.h ##
    ${GLOBAL_GLSL_DIR}/../*.glsl
  )
  file(GLOB_RECURSE ShadersDependencies
    ${GLOBAL_GLSL_DIR}/*.h ##
  )

  file(GLOB_RECURSE ShadersDependencies_bis
    ${extra_dir}/*.h
    ${extra_dir}/*.glsl
  )
  list(APPEND ShadersDependencies ${ShadersDependencies_bis})
  # foreach(dep IN LISTS ShadersDependencies_bis)
  # endforeach()

  # transform shader path to relative
  foreach(glslshader IN LISTS g_ShadersGLSL)
    file(RELATIVE_PATH glslshader 
      ${GLOBAL_GLSL_DIR}
      ${glslshader}
    )
    list(APPEND ShadersGLSL ${glslshader})
  endforeach()

  # Convert each GLSL shaders into a SpirV binary
  foreach(glslshader IN LISTS ShadersGLSL)
    # set global output binary filename
    set(source ${GLOBAL_GLSL_DIR}/${glslshader})
    set(binary ${GLOBAL_SPIRV_DIR}/${glslshader}.spv)

    # compile GLSL to SPIRV
    glsl2spirv(${source} ${binary} ${GLOBAL_GLSL_DIR} "${ShadersDependencies}" -I${extra_dir})

    # return the list of compiled filed
    list(APPEND glslSHADERS ${source})
    list(APPEND spirvSHADERS ${binary})
  endforeach()

  set(${sources} "${glslSHADERS}" PARENT_SCOPE)
  set(${binaries} "${spirvSHADERS}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------