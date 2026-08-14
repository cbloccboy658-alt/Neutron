add_executable(ssrc cli.cpp)
target_link_libraries(ssrc shibatchdsp ${SLEEF_LIBRARIES})
target_include_directories(ssrc PRIVATE "${PROJECT_SOURCE_DIR}/src/libshibatchdsp")

add_executable(scsa scsa.cpp)
target_link_libraries(scsa shibatchdsp ${SLEEF_LIBRARIES})
target_include_directories(scsa PRIVATE "${PROJECT_SOURCE_DIR}/src/libshibatchdsp")

install(
  TARGETS ssrc scsa
  DESTINATION "${INSTALL_BINDIR}"
  COMPONENT Runtime
)

install(
  FILES ssrc.1 scsa.1
  DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
  COMPONENT Runtime
)
