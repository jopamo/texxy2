set(QT6_MINIMUM_VERSION "6.2.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport DBus)

# for spell checking (see FindHUNSPELL.cmake)
find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)

# optional localization
find_package(Qt6 QUIET COMPONENTS LinguistTools)

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(X11 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt6Core_INCLUDE_DIRS}
                      ${Qt6Gui_INCLUDE_DIRS}
                      ${Qt6Widgets_INCLUDE_DIRS}
                      ${Qt6Svg_INCLUDE_DIRS}
                      ${Qt6PrintSupport_INCLUDE_DIRS}
                      ${Qt6DBus_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR}
                      ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_X11)
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt6Core_INCLUDE_DIRS}
                      ${Qt6Gui_INCLUDE_DIRS}
                      ${Qt6Widgets_INCLUDE_DIRS}
                      ${Qt6Svg_INCLUDE_DIRS}
                      ${Qt6PrintSupport_INCLUDE_DIRS}
                      ${Qt6DBus_INCLUDE_DIRS}
                      ${HUNSPELL_INCLUDE_DIRS})
endif()

add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")

set(featherpad_SRCS
    main.cpp
    singleton.cpp
    signalDaemon.cpp
    fpwin.cpp
    encoding.cpp
    tabwidget.cpp
    menubartitle.cpp
    lineedit.cpp
    textedit.cpp
    tabbar.cpp
    find.cpp
    replace.cpp
    pref.cpp
    config.cpp
    brackets.cpp
    syntax.cpp
    vscrollbar.cpp
    loading.cpp
    printing.cpp
    tabpage.cpp
    searchbar.cpp
    session.cpp
    fontDialog.cpp
    sidepane.cpp
    svgicons.cpp
    spellChecker.cpp
    spellDialog.cpp
    highlighter/highlighter.cpp
    highlighter/highlighter-cmake.cpp
    highlighter/highlighter-css.cpp
    highlighter/highlighter-fountain.cpp
    highlighter/highlighter-java.cpp
    highlighter/highlighter-json.cpp
    highlighter/highlighter-html.cpp
    highlighter/highlighter-lua.cpp
    highlighter/highlighter-markdown.cpp
    highlighter/highlighter-pascal.cpp
    highlighter/highlighter-patterns.cpp
    highlighter/highlighter-perl-regex.cpp
    highlighter/highlighter-regex.cpp
    highlighter/highlighter-rest.cpp
    highlighter/highlighter-ruby.cpp
    highlighter/highlighter-rust.cpp
    highlighter/highlighter-sh.cpp
    highlighter/highlighter-tcl.cpp
    highlighter/highlighter-toml.cpp
    highlighter/highlighter-xml.cpp
    highlighter/highlighter-yaml.cpp)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(featherpad_SRCS ${featherpad_SRCS} x11.cpp)
endif()

qt6_add_dbus_adaptor(featherpad_DBUS_SRCS
    org.featherpad.Application.xml
    singleton.h
    FeatherPad::FPsingleton
    featherpadadaptor
    FeatherPadAdaptor
)

set(featherpad_RESOURCES data/fp.qrc)
set(DESKTOP_FILE data/featherpad.desktop)
set(METAINFO_FILE data/featherpad.metainfo.xml)
set(ICON_FILE data/featherpad.svg)
set(HELP_FILE data/help)
# file(GLOB HELP_TR_FILES data/help_*)

set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)

# translations
if(Qt6LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/featherpad_*.ts)
  qt6_add_translation(QM_FILES ${TS_FILES})
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_DBUS_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad/translations")
  endif()
else(Qt6LinguistTools_FOUND)
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_DBUS_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt6LinguistTools_FOUND)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(featherpad ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
                                   ${X11_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
else()
  target_link_libraries(featherpad ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
endif()

# make symlink
add_custom_target(fpad_symlink ALL ${CMAKE_COMMAND} -E create_symlink featherpad fpad DEPENDS featherpad COMMENT "Creating fpad as a symlink to featherpad")

install(TARGETS featherpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${METAINFO_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fpad DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
