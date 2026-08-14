add_executable(test_cppapi test_cppapi.cpp)
target_link_libraries(test_cppapi shibatchdsp ${SLEEF_LIBRARIES})

add_executable(test_soxrapi test_soxrapi.c)
target_link_libraries(test_soxrapi shibatchdsp ${SLEEF_LIBRARIES})
target_include_directories(test_soxrapi PRIVATE "${PROJECT_SOURCE_DIR}/src/libshibatchdsp")

add_executable(test_oneshot test_oneshot.c)
target_link_libraries(test_oneshot shibatchdsp ${SLEEF_LIBRARIES})
target_include_directories(test_oneshot PRIVATE "${PROJECT_SOURCE_DIR}/src/libshibatchdsp")

add_executable(cmpwav cmpwav.cpp)
target_link_libraries(cmpwav shibatchdsp ${SLEEF_LIBRARIES})
target_include_directories(cmpwav PRIVATE "${PROJECT_SOURCE_DIR}/src/libshibatchdsp")

set(TMP_DIR_PATH ${EXECUTABLE_OUTPUT_PATH})

add_custom_command(
  OUTPUT ${TMP_DIR_PATH}/noise.44100.wav
  COMMAND $<TARGET_FILE:ssrc> --dither 99 --pdf 0 10000 --genSweep 44100 2 100000 0 0 --rate 44100 --bits 16 "${TMP_DIR_PATH}/noise.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --dither 99 --pdf 0 10000 --genSweep 48000 2 100000 0 0 --rate 48000 --bits 16 "${TMP_DIR_PATH}/noise.48000.wav"
  COMMAND $<TARGET_FILE:ssrc> --dither 99 --pdf 0 10000 --genSweep 44100 2 3000000 0 0 --rate 44100 --bits 16 "${TMP_DIR_PATH}/longnoise.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --dither 99 --pdf 0 10000 --genSweep 48000 2 3000000 0 0 --rate 48000 --bits 16 "${TMP_DIR_PATH}/longnoise.48000.wav"
  COMMAND $<TARGET_FILE:ssrc> --dither 99 --pdf 0 10000 --genSweep 44100 32 100000 0 0 --rate 44100 --bits 16 "${TMP_DIR_PATH}/noise.32ch.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --dither 99 --pdf 0 10000 --genSweep 48000 32 100000 0 0 --rate 48000 --bits 16 "${TMP_DIR_PATH}/noise.32ch.48000.wav"
  COMMAND $<TARGET_FILE:ssrc> --genSweep 44100 2 441000 10000 10000 --rate 44100 --bits -64 "${TMP_DIR_PATH}/sin10k.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --genSweep 44100 2 441000 12000 12000 --rate 44100 --bits -64 "${TMP_DIR_PATH}/sin12k.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --genSweep 48000 2 480000 10000 10000 --rate 48000 --bits -64 "${TMP_DIR_PATH}/sin10k.48000.wav"
  COMMAND $<TARGET_FILE:ssrc> --genSweep 96000 2 960000 10000 10000 --rate 96000 --bits -64 "${TMP_DIR_PATH}/sin10k.96000.wav"
  COMMAND $<TARGET_FILE:ssrc> --genSweep 44100 1 441000 10000 10000 --rate 44100 --bits -64 "${TMP_DIR_PATH}/sin10k.mono.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --genImpulse 44100 1 50000 "${TMP_DIR_PATH}/impulse.44100.wav"
  COMMAND $<TARGET_FILE:ssrc> --genImpulse 48000 1 50000 "${TMP_DIR_PATH}/impulse.48000.wav"
  COMMAND $<TARGET_FILE:ssrc> --genImpulse 44100 1 1024 "${TMP_DIR_PATH}/impulse.short.44100.wav"
  COMMENT "Generating wav files for tests"
)

add_custom_target(
  generate_test_data
  DEPENDS ${TMP_DIR_PATH}/noise.44100.wav
)

add_dependencies(test_cppapi generate_test_data)

add_test(NAME test_api COMMAND "${CMAKE_COMMAND}"
  -D TARGET_FILE_ssrc=$<TARGET_FILE:ssrc>
  -D TARGET_FILE_scsa=$<TARGET_FILE:scsa>
  -D TARGET_FILE_test_cppapi=$<TARGET_FILE:test_cppapi>
  -D TARGET_FILE_test_soxrapi=$<TARGET_FILE:test_soxrapi>
  -D TARGET_FILE_test_oneshot=$<TARGET_FILE:test_oneshot>
  -D TARGET_FILE_cmpwav=$<TARGET_FILE:cmpwav>
  -D TMP_DIR_PATH=${TMP_DIR_PATH}
  -P ${CMAKE_CURRENT_LIST_DIR}/test_api.cmake
)

foreach(PROFILE standard long high insane)
  add_test(NAME test_sin10k_44100_48000_${PROFILE} COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--st\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.wav\;100000\;460000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_48000_44100_${PROFILE} COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.wav\;100000\;420000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.partConv.wav\;100000\;460000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--st\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.partConv.wav\;100000\;420000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_noise_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/noise.44100.wav\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/noise.44100.wav\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.wav\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_noise_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/noise.48000.wav\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/noise.48000.wav\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.wav\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_longnoise_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.44100.wav\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.44100.wav\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.wav\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )
  set_tests_properties(test_longnoise_44100_48000_${PROFILE}_partConv PROPERTIES COST 100.0)

  add_test(NAME test_longnoise_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.48000.wav\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.48000.wav\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.wav\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )
  set_tests_properties(test_longnoise_48000_44100_${PROFILE}_partConv PROPERTIES COST 100.0)
endforeach()

foreach(PROFILE standard long)
  add_test(NAME test_sin10k_44100_48000_${PROFILE}_minPhase COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--minPhase\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.minPhase.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.minPhase.wav\;100000\;460000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_48000_44100_${PROFILE}_minPhase COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--minPhase\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.minPhase.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.minPhase.wav\;100000\;420000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )
endforeach()

foreach(PROFILE lightning fast short)
  add_test(NAME test_sin10k_44100_48000_${PROFILE} COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--st\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-100dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.wav\;100000\;460000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_48000_44100_${PROFILE} COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-100dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.wav\;100000\;420000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_44100_48000_${PROFILE}_minPhase COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--minPhase\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.minPhase.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-100dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.minPhase.wav\;100000\;460000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_48000_44100_${PROFILE}_minPhase COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--st\;--minPhase\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.minPhase.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-100dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.minPhase.wav\;100000\;420000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-100dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${PROFILE}.partConv.wav\;100000\;460000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_sin10k_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-100dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.44100.${PROFILE}.partConv.wav\;100000\;420000\;10000
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_noise_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/noise.44100.wav\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/noise.44100.wav\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.wav\;${TMP_DIR_PATH}/noise.44100.48000.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_noise_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/noise.48000.wav\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/noise.48000.wav\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.wav\;${TMP_DIR_PATH}/noise.48000.44100.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_longnoise_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.44100.wav\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.44100.wav\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.wav\;${TMP_DIR_PATH}/longnoise.44100.48000.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_longnoise_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.48000.wav\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/longnoise.48000.wav\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.wav\;${TMP_DIR_PATH}/longnoise.48000.44100.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_noise32ch_44100_48000_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/noise.32ch.44100.wav\;${TMP_DIR_PATH}/noise.32ch.44100.48000.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/noise.32ch.44100.wav\;${TMP_DIR_PATH}/noise.32ch.44100.48000.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/noise.32ch.44100.48000.${PROFILE}.wav\;${TMP_DIR_PATH}/noise.32ch.44100.48000.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )

  add_test(NAME test_noise32ch_48000_44100_${PROFILE}_partConv COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/noise.32ch.48000.wav\;${TMP_DIR_PATH}/noise.32ch.48000.44100.${PROFILE}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;${PROFILE}\;--partConv\;8\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/noise.32ch.48000.wav\;${TMP_DIR_PATH}/noise.32ch.48000.44100.${PROFILE}.partConv.wav
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/noise.32ch.48000.44100.${PROFILE}.wav\;${TMP_DIR_PATH}/noise.32ch.48000.44100.${PROFILE}.partConv.wav\;0.0001
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )
endforeach()

add_test(NAME test_sin10k_44100_48000_24bit COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;48000\;--bits\;24\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.24bit.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.24bit.wav\;100000\;460000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

foreach(CONTAINER riff w64 rf64)
  add_test(NAME test_sin10k_44100_48000_${CONTAINER} COMMAND "${CMAKE_COMMAND}"
    -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;48000\;--bits\;24\;--dstContainer\;${CONTAINER}\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.${CONTAINER}.wav
    -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.${CONTAINER}.wav\;100000\;460000\;10000
    -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;--check-container\;${TMP_DIR_PATH}/sin10k.44100.48000.${CONTAINER}.wav\;${CONTAINER}
    -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
  )
endforeach()

add_test(NAME test_sin10k_96000_44100_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.96000.wav\;${TMP_DIR_PATH}/sin10k.96000.44100.standard.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.96000.44100.standard.wav\;100000\;420000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_sin10k_96000_48000_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.96000.wav\;${TMP_DIR_PATH}/sin10k.96000.48000.standard.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.96000.48000.standard.wav\;100000\;420000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_sin10k_44100_96000_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;96000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.96000.standard.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--log2dftlen\;14\;--svgout\;${TMP_DIR_PATH}/error.svg\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.96000.standard.wav\;100000\;840000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_sin10k_48000_96000_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;96000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.48000.wav\;${TMP_DIR_PATH}/sin10k.48000.96000.standard.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--log2dftlen\;14\;--svgout\;${TMP_DIR_PATH}/error.svg\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.48000.96000.standard.wav\;100000\;840000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_sin10k_mono_44100_48000_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.mono.44100.wav\;${TMP_DIR_PATH}/sin10k.mono.44100.48000.standard.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.mono.44100.48000.standard.wav\;100000\;460000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_short_impulse_44100_48000_stability COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--rate\;48000\;${TMP_DIR_PATH}/impulse.short.44100.wav\;${TMP_DIR_PATH}/impulse.short.44100.48000.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--log2dftlen\;8\;--svgout\;${TMP_DIR_PATH}/impulse.short.svg\;${TMP_DIR_PATH}/impulse.short.44100.48000.wav\;512
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

foreach(FS 44100 48000)
  foreach(BITDEPTH 8 16)
    foreach(DITHERTYPE 0 6 98 99)
      add_test(NAME test_sin10k_${FS}i${BITDEPTH}_dither_${DITHERTYPE} COMMAND "${CMAKE_COMMAND}"
	-D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--genSweep\;${FS}\;1\;480000\;10000\;10000\;--rate\;${FS}\;--bits\;${BITDEPTH}\;--dither\;${DITHERTYPE}\;${TMP_DIR_PATH}/sin10k.${FS}i${BITDEPTH}_d${DITHERTYPE}.wav
	-D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--svgout\;${TMP_DIR_PATH}/error.svg\;--check\;${CMAKE_CURRENT_LIST_DIR}/sin10k_${FS}i${BITDEPTH}_d${DITHERTYPE}.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/sin10k.${FS}i${BITDEPTH}_d${DITHERTYPE}.wav\;200000
	-P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
      )
    endforeach()
  endforeach()
endforeach()

add_test(NAME test_impulse_48000_44100_insane COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;insane\;--bits\;-64\;--rate\;44100\;${TMP_DIR_PATH}/impulse.48000.wav\;${TMP_DIR_PATH}/impulse.insane.48000.44100.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.48000.44100.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.insane.48000.44100.wav\;90000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_48000_44100_long COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;long\;--minPhase\;--bits\;-64\;--rate\;44100\;${TMP_DIR_PATH}/impulse.48000.wav\;${TMP_DIR_PATH}/impulse.long.48000.44100.minPhase.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.48000.44100.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.long.48000.44100.minPhase.wav\;50000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_48000_44100_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--bits\;-64\;--rate\;44100\;${TMP_DIR_PATH}/impulse.48000.wav\;${TMP_DIR_PATH}/impulse.standard.48000.44100.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.48000.44100.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.standard.48000.44100.wav\;51000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_48000_44100_standard_minPhase COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--minPhase\;--bits\;-64\;--rate\;44100\;${TMP_DIR_PATH}/impulse.48000.wav\;${TMP_DIR_PATH}/impulse.standard.48000.44100.minPhase.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.48000.44100.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.standard.48000.44100.minPhase.wav\;50000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_44100_48000_insane COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;insane\;--bits\;-64\;--rate\;48000\;${TMP_DIR_PATH}/impulse.44100.wav\;${TMP_DIR_PATH}/impulse.insane.44100.48000.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.44100.48000.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.insane.44100.48000.wav\;94000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_44100_48000_long_minPhase COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;long\;--minPhase\;--bits\;-64\;--rate\;48000\;${TMP_DIR_PATH}/impulse.44100.wav\;${TMP_DIR_PATH}/impulse.long.44100.48000.minPhase.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.44100.48000.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.long.44100.48000.minPhase.wav\;50000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_44100_48000_standard COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--bits\;-64\;--rate\;48000\;${TMP_DIR_PATH}/impulse.44100.wav\;${TMP_DIR_PATH}/impulse.standard.44100.48000.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.44100.48000.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.standard.44100.48000.wav\;53000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_impulse_44100_48000_standard_minPhase COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--minPhase\;--bits\;-64\;--rate\;48000\;${TMP_DIR_PATH}/impulse.44100.wav\;${TMP_DIR_PATH}/impulse.standard.44100.48000.minPhase.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/impulse.44100.48000.scsa\;--log2dftlen\;16\;${TMP_DIR_PATH}/impulse.standard.44100.48000.minPhase.wav\;50000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_mix_channels_stereo_to_mono COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;0.5,0.5\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.48000.mono.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;--check-channels\;${TMP_DIR_PATH}/sin10k.44100.48000.mono.wav\;1
  -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:scsa>\;--check\;${CMAKE_CURRENT_LIST_DIR}/10kHz-140dB.scsa\;${TMP_DIR_PATH}/sin10k.44100.48000.mono.wav\;100000\;460000\;10000
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_mix_channels_mono_to_stereo COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;1_1\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.mono.44100.wav\;${TMP_DIR_PATH}/sin10k.mono.44100.stereo.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;--check-channels\;${TMP_DIR_PATH}/sin10k.mono.44100.stereo.wav\;2
  -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;1,0\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.mono.44100.stereo.wav\;${TMP_DIR_PATH}/sin10k.mono.44100.48000.stereo.ch0.wav
  -D COMMAND3_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;0,1\;--rate\;48000\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.mono.44100.stereo.wav\;${TMP_DIR_PATH}/sin10k.mono.44100.48000.stereo.ch1.wav
  -D COMMAND4_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/sin10k.mono.44100.48000.stereo.ch0.wav\;${TMP_DIR_PATH}/sin10k.mono.44100.48000.stereo.ch1.wav\;0.0001
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_mix_channels_swap_stereo_channels COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;0,1_1,0\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.swapped.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;1,0\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.ch0.wav
  -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;0,1\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.ch1.wav
  -D COMMAND3_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;1,0\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.swapped.wav\;${TMP_DIR_PATH}/sin10k.44100.swapped.ch0.wav
  -D COMMAND4_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;0,1\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.swapped.wav\;${TMP_DIR_PATH}/sin10k.44100.swapped.ch1.wav
  -D COMMAND5_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/sin10k.44100.ch0.wav\;${TMP_DIR_PATH}/sin10k.44100.swapped.ch1.wav\;0.0001
  -D COMMAND6_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/sin10k.44100.ch1.wav\;${TMP_DIR_PATH}/sin10k.44100.swapped.ch0.wav\;0.0001
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(NAME test_mix_channels_mslr_roundtrip COMMAND "${CMAKE_COMMAND}"
  -D COMMAND0_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;1,1_1,-1\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.ms.wav
  -D COMMAND1_TO_EXECUTE=$<TARGET_FILE:ssrc>\;--profile\;standard\;--mixChannels\;0.5,0.5_0.5,-0.5\;--rate\;44100\;--bits\;-64\;${TMP_DIR_PATH}/sin10k.44100.ms.wav\;${TMP_DIR_PATH}/sin10k.44100.lrrestored.wav
  -D COMMAND2_TO_EXECUTE=$<TARGET_FILE:cmpwav>\;${TMP_DIR_PATH}/sin10k.44100.wav\;${TMP_DIR_PATH}/sin10k.44100.lrrestored.wav\;0.0001
  -P ${CMAKE_CURRENT_LIST_DIR}/execute_commands.cmake
)

add_test(
  NAME test_mix_channels_invalid_matrix
  COMMAND $<TARGET_FILE:ssrc> --mixChannels 1,2,3 ${TMP_DIR_PATH}/sin10k.44100.wav ${TMP_DIR_PATH}/dummy.wav
)
set_tests_properties(test_mix_channels_invalid_matrix PROPERTIES WILL_FAIL true)

add_test(
  NAME test_invalid_param_partconv
  COMMAND $<TARGET_FILE:ssrc> --partConv 20 ${TMP_DIR_PATH}/sin10k.44100.wav ${TMP_DIR_PATH}/dummy.wav
)
set_tests_properties(test_invalid_param_partconv PROPERTIES WILL_FAIL true)
