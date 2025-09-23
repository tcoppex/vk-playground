# -----------------------------------------------------------------------------
# 
#  Android samples
# 
# -----------------------------------------------------------------------------

function(add_activity_target suffix)
  if(NOT DEFINED ENV{ANDROID_SDK})
    message(STATUS "Environment variable ANDROID_SDK is not set.")
  else()
    set(ADB_CMD               $ENV{ANDROID_SDK}/platform-tools/adb)
    set(SAMPLES_PACKAGE_NAME  com.polysfaer.vkframework.samples)
    set(target launch_${suffix})

    set(ADB_RESOLVE_CMD
      "${ADB_CMD} shell cmd package resolve-activity --brief ${SAMPLES_PACKAGE_NAME}.${suffix}"
    )
    set(ADB_LAUNCH_CMD
      "${ADB_CMD} shell am start -n \$ACTIVITY"
    )
    set(ADB_RESOLVE_AND_LAUNCH_CMD
      "ACTIVITY=\$(${ADB_RESOLVE_CMD} | awk 'NR==2 {print \$NF}'); ${ADB_LAUNCH_CMD}"
    )

    add_custom_target(build_${suffix}
      COMMAND
        ./gradlew installDebug
      WORKING_DIRECTORY
        ${ANDROID_BUILD_DIR}
      VERBATIM
    )

    add_custom_target(run_${suffix}
      COMMAND
        ${CMAKE_COMMAND} -E env bash -c "${ADB_RESOLVE_AND_LAUNCH_CMD}"
      DEPENDS
        build_${suffix}
      WORKING_DIRECTORY
        ${ANDROID_BUILD_DIR}
      VERBATIM
    )
  endif()
endfunction()

add_activity_target(aloha)

function(add_android_sample target)
  add_activity_target(target)
  if(ANDROID)
    add_subdirectory(aloha)
  endif()
endfunction()

# -----------------------------------------------------------------------------

add_android_sample(aloha)

# -----------------------------------------------------------------------------
