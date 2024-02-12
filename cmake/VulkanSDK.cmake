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
    find_program(GLSLANGVALIDATOR glslangValidator
      "$ENV{VULKAN_SDK}/Bin"
      "$ENV{VK_SDK_PATH}/Bin")
  else()
    find_program(GLSLANGVALIDATOR glslangValidator
      "$ENV{VULKAN_SDK}/Bin32"
      "$ENV{VK_SDK_PATH}/Bin32")
  endif()
else()
    find_program(GLSLANGVALIDATOR glslangValidator
      "$ENV{VULKAN_SDK}/bin")
endif()


## Custom function to generate binary shaders from GLSL, with include handling.
function(glsl2spirv input_glsl output_spirv shader_dir)
  # Retrieve the input file name
  get_filename_component(fn ${input_glsl} NAME)
  
  # Detects shader type based on its suffix or prefix
  if    (${fn} MATCHES "((vert|vs)_.+\\.glsl)|(.+\\.(vert|vs))")
    set(stage "vert")
  elseif(${fn} MATCHES "((tesc|tcs)_.+\\.glsl)|(.+\\.(tesc|tcs))")
    set(stage "tesc")
  elseif(${fn} MATCHES "((tese|tes)_.+\\.glsl)|(.+\\.(tese|tes))")
    set(stage "tese")
  elseif(${fn} MATCHES "((geom|gs)_.+\\.glsl)|(.+\\.(geom|gs))")
    set(stage "geom")
  elseif(${fn} MATCHES "((frag|fs)_.+\\.glsl)|(.+\\.(frag|fs))")
    set(stage "frag")
  elseif(${fn} MATCHES "((comp|cs)_.+\\.glsl)|(.+\\.(comp|cs))")
    set(stage "comp")
  else()
    message(WARNING "Unknown shaer type for ${fn}")
    return()
  endif()

  # Compile to SPIR-V with include directory set to shaderdir
  add_custom_command(
    OUTPUT
      ${output_spirv}
    COMMAND
      ${GLSLANGVALIDATOR} -I${shader_dir} -V ${input_glsl} -o ${output_spirv} -S ${stage}
    DEPENDS
      ${input_glsl}
      ${GLSLANGVALIDATOR}
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

# Compile all shader from one directory to another
function(compile_shaders GLOBAL_GLSL_DIR GLOBAL_SPIRV_DIR binaries sources)
  file(MAKE_DIRECTORY ${GLOBAL_SPIRV_DIR})

  # retrieve all SOURCE glsl shaders
  file(GLOB_RECURSE g_ShadersGLSL ${GLOBAL_GLSL_DIR}/*.*)

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
    glsl2spirv(${source} ${binary} ${GLOBAL_GLSL_DIR})

    # return the list of compiled filed
    list(APPEND glslSHADERS ${source})
    list(APPEND spirvSHADERS ${binary})
  endforeach()

  set(${sources} "${glslSHADERS}" PARENT_SCOPE)
  set(${binaries} "${spirvSHADERS}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------

if(0)
  if (WIN32)
    if (CMAKE_CL_64)
      find_program(GLSL_LANG_VALIDATOR glslangValidator
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin")
    else()
      find_program(GLSL_LANG_VALIDATOR glslangValidator
        "$ENV{VULKAN_SDK}/Bin32"
        "$ENV{VK_SDK_PATH}/Bin32")
    endif()
  else()
      find_program(GLSL_LANG_VALIDATOR glslangValidator
        "$ENV{VULKAN_SDK}/bin")
  endif()

  # convert a GLSL shader to its SPIRV version
  macro(glsl_to_spirv input_glsl output_spirv)  
    add_custom_command(
      OUTPUT
        ${output_spirv}
      COMMAND
        ${GLSL_LANG_VALIDATOR} -s -V -o ${output_spirv} ${input_glsl}
      DEPENDS
        ${input_glsl}
        ${GLSL_LANG_VALIDATOR}
      WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
      COMMENT
        "Converting shader ${input_glsl} to ${output_spirv}" VERBATIM
      SOURCES
        ${input_glsl}
    )
  endmacro(glsl_to_spirv)
endif()

# -----------------------------------------------------------------------------