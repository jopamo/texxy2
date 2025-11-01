set(QT6_MINIMUM_VERSION "6.2.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport DBus)

# for spell checking (see FindHUNSPELL.cmake)
find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)

# do not search for Qt6 LinguistTools since translations are disabled

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

set(texxy_SRCS
    main.cpp
    singleton.cpp
    signalDaemon.cpp
    texxy.cpp
    texxy_edit.cpp
    texxy_tabs.cpp
    save.cpp
    encoding.cpp
    tabwidget.cpp
    menubartitle.cpp
    lineedit.cpp
    textedit.cpp
    textedit_clipboard.cpp
    textedit_column.cpp
    textedit_sort.cpp
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
    highlighter/highlighter-yaml.cpp
)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  list(APPEND texxy_SRCS x11.cpp)
endif()

qt6_add_dbus_adaptor(texxy_DBUS_SRCS
    org.texxy.Application.xml
    singleton.h
    Texxy::TexxyApplication
    texxyadaptor
    TexxyAdaptor
)

set(texxy_RESOURCES data/texxy.qrc)
set(DESKTOP_FILE data/texxy.desktop)
set(METAINFO_FILE data/texxy.metainfo.xml)
set(ICON_FILE data/texxy.svg)
set(HELP_FILE data/help)
# file(GLOB HELP_TR_FILES data/help_*)

set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h texxywindow.h)

# translations fully disabled, do not generate or install QM files
add_executable(texxy
    ${texxy_SRCS}
    ${texxy_DBUS_SRCS}
    ${texxy_RESOURCES}
    ${EXTRA_HEADERS}
    ${DESKTOP_FILE}
)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(texxy ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
                                   ${X11_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
else()
  target_link_libraries(texxy ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
endif()

install(TARGETS texxy RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${METAINFO_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/texxy")
