set(COUNTER 0)

while(1)
  string(CONCAT COMMAND_VAR_NAME "COMMAND" ${COUNTER} "_TO_EXECUTE")

  if(DEFINED ${COMMAND_VAR_NAME})
    execute_process(
      COMMAND ${${COMMAND_VAR_NAME}}
      COMMAND_ERROR_IS_FATAL ANY
      COMMAND_ECHO STDOUT
    )
    math(EXPR COUNTER "${COUNTER} + 1")
  else()
    break()
  endif()
endwhile()
