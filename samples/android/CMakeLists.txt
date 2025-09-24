# -----------------------------------------------------------------------------
# 
#  Android samples
# 
# -----------------------------------------------------------------------------

set(SAMPLES_PACKAGE_NAME  com.polysfaer.vkframework.samples)

# -----------------------------------------------------------------------------

if(NOT DEFINED ENV{ANDROID_SDK})
  message(STATUS "Environment variable ANDROID_SDK is not set.")
endif()

# -----------------------------------------------------------------------------

function(add_activity_target suffix)
  if(DEFINED ENV{ANDROID_SDK})
    set(ADB_CMD $ENV{ANDROID_SDK}/platform-tools/adb)

    set(PACKAGE_NAME ${SAMPLES_PACKAGE_NAME}.${suffix})
    set(ADB_RESOLVE_CMD
      "${ADB_CMD} shell cmd package resolve-activity --brief ${PACKAGE_NAME}"
    )
    set(ADB_LAUNCH_CMD
      "${ADB_CMD} shell am start -n \$ACTIVITY"
    )
    set(ADB_RESOLVE_AND_LAUNCH_CMD
      "ACTIVITY=\$(${ADB_RESOLVE_CMD} | awk 'NR==2 {print \$NF}'); ${ADB_LAUNCH_CMD}"
    )

    add_custom_target(build_${suffix}
      COMMAND
        ./gradlew :${suffix}:buildDebug
      WORKING_DIRECTORY
        ${ANDROID_BUILD_DIR}
      COMMENT
        "Build Android DEBUG target \"${suffix}\"."
      VERBATIM
    )

    add_custom_target(install_${suffix}
      COMMAND
        ./gradlew :${suffix}:installDebug
      WORKING_DIRECTORY
        ${ANDROID_BUILD_DIR}
      COMMENT
        "Install Android DEBUG target \"${suffix}\" on device."
      VERBATIM
    )

    add_custom_target(run_${suffix}
      COMMAND
        ${CMAKE_COMMAND} -E env bash -c "${ADB_RESOLVE_AND_LAUNCH_CMD}"
      DEPENDS
        install_${suffix}
      WORKING_DIRECTORY
        ${ANDROID_BUILD_DIR}
      COMMENT
        "Run Android DEBUG target \"${suffix}\" on device."
      VERBATIM
    )

    set(LOGGER_ANDROID_TAG VkFramework)
    set(ADB_LOGCAT_CMD
      ${ADB_CMD} logcat -v time -v color
    )

    add_custom_target(log_${suffix}
      COMMAND
        ${ADB_CMD} logcat -c
      COMMAND
        # ${ADB_LOGCAT_CMD} | grep --line-buffered "${SAMPLES_PACKAGE_NAME}"
        # ${ADB_LOGCAT_CMD} | grep ${LOGGER_ANDROID_TAG}
        ${ADB_LOGCAT_CMD} *:S ${LOGGER_ANDROID_TAG}:D | tee ${suffix}.log
      DEPENDS
        run_${suffix}
      WORKING_DIRECTORY
        ${ANDROID_BUILD_DIR}
      COMMENT
        "Log Android target \"${suffix}\" on device."
      VERBATIM
    )
  endif()
endfunction()

function(add_android_sample target)
  add_activity_target(${target})
  if(ANDROID)
    add_subdirectory(${target})
  endif()
endfunction()

# -----------------------------------------------------------------------------

add_android_sample(aloha)

# -----------------------------------------------------------------------------
