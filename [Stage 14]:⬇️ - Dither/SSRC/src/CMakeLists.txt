add_subdirectory("libshibatchdsp")
if (BUILD_CLI)
  add_subdirectory("cli")
endif()
if (BUILD_TESTS)
  add_subdirectory("tester") # tester has to come at last
endif()
