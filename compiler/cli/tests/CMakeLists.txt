# The CLI has no unit tests; the end-to-end smoke covers build -> link -> run.
if(UNIX)
  add_test(
    NAME cli_smoke
    COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke.sh $<TARGET_FILE:solid>
  )
endif()
