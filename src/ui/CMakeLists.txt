add_subdirectory(ui)
add_subdirectory(dialogs)
add_subdirectory(icons)

target_sources(texxy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/texxywindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texxywindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/texxywindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/window_documents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window_edit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window_sessions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window_tabs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/messagebox.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/warningbar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/filedialog.h
)
