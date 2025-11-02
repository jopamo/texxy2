set(platform_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/singleton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/singleton.h
    ${CMAKE_CURRENT_SOURCE_DIR}/signalDaemon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/signalDaemon.h
    ${CMAKE_CURRENT_SOURCE_DIR}/window_utils.cpp
)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  list(APPEND platform_sources
       ${CMAKE_CURRENT_SOURCE_DIR}/x11.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/x11.h)
endif()

target_sources(texxy PRIVATE ${platform_sources})
