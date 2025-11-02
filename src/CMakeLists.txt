set(QT6_MINIMUM_VERSION "6.2.0")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/ui/ui
    ${PROJECT_SOURCE_DIR}/src/ui/dialogs
)

find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport DBus)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(X11 REQUIRED)
endif()

add_executable(texxy)

target_include_directories(texxy PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/ui
    ${PROJECT_SOURCE_DIR}/src/ui/ui
    ${PROJECT_SOURCE_DIR}/src/ui/dialogs
    ${PROJECT_SOURCE_DIR}/src/ui/common
    ${PROJECT_SOURCE_DIR}/src/ui/icons
    ${PROJECT_SOURCE_DIR}/src/features
    ${PROJECT_SOURCE_DIR}/src/features/textedit
    ${PROJECT_SOURCE_DIR}/src/features/highlighter
    ${PROJECT_SOURCE_DIR}/src/features/search
    ${PROJECT_SOURCE_DIR}/src/features/syntax
    ${PROJECT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(texxy PRIVATE DATADIR="${CMAKE_INSTALL_PREFIX}/share")

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_include_directories(texxy PRIVATE ${X11_INCLUDE_DIR})
  target_compile_definitions(texxy PRIVATE HAS_X11)
endif()

qt6_add_dbus_adaptor(texxy_DBUS_SRCS
    ${PROJECT_SOURCE_DIR}/src/platform/org.texxy.Application.xml
    ${PROJECT_SOURCE_DIR}/src/platform/singleton.h
    Texxy::TexxyApplication
    texxyadaptor
    TexxyAdaptor
)

set(_texxy_dbus_absolute)
foreach(src ${texxy_DBUS_SRCS})
  if(IS_ABSOLUTE "${src}")
    list(APPEND _texxy_dbus_absolute "${src}")
  else()
    list(APPEND _texxy_dbus_absolute "${CMAKE_CURRENT_BINARY_DIR}/${src}")
  endif()
endforeach()
set(texxy_DBUS_SRCS ${_texxy_dbus_absolute})
set_source_files_properties(${texxy_DBUS_SRCS} PROPERTIES GENERATED TRUE)
target_sources(texxy PRIVATE ${texxy_DBUS_SRCS})

add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(features)
add_subdirectory(platform)

set(texxy_RESOURCES ${PROJECT_SOURCE_DIR}/resources/qrc/texxy.qrc)
target_sources(texxy PRIVATE ${texxy_RESOURCES})

target_link_libraries(texxy PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::PrintSupport
    Qt6::DBus
)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(texxy PRIVATE ${X11_LIBRARIES})
endif()

install(TARGETS texxy RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
