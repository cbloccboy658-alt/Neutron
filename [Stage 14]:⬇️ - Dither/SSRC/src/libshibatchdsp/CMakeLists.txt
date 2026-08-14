add_library(shibatchdsp libssrc.cpp ssrcsoxr.cpp xdr_wav.cpp)
add_dependencies(shibatchdsp ext_sleef)

set_target_properties(shibatchdsp PROPERTIES
  VERSION ${SSRC_VERSION}
  SOVERSION ${SSRC_SOVERSION}
  PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/src/include/shibatch/ssrc.hpp;${PROJECT_SOURCE_DIR}/src/include/shibatch/shapercoefs.h;${PROJECT_SOURCE_DIR}/src/include/shibatch/ssrcsoxr.h"
  )

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/shibatchdsp.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/shibatchdsp.pc" @ONLY)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/shibatchdsp.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
  COMPONENT shibatch_Development
)

install(
  TARGETS shibatchdsp
  DESTINATION "${INSTALL_LIBDIR}"
  PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/shibatch"
  COMPONENT shibatch_Development
)
