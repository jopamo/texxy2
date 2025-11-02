set(DESKTOP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/texxy.desktop)
set(METAINFO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/texxy.metainfo.xml)
set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/texxy.svg)
set(HELP_FILE ${PROJECT_SOURCE_DIR}/resources/help/help)

install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
install(FILES ${METAINFO_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/texxy")
