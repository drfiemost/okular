add_subdirectory( conf )

set (HAVE_POPPLER_0_12_1 1)
set (HAVE_POPPLER_0_16 1)
set (HAVE_POPPLER_0_20 1)
set (HAVE_POPPLER_0_22 1)
set (HAVE_POPPLER_0_24 1)
set (HAVE_POPPLER_0_28 1)
set (HAVE_POPPLER_0_36 1)
set (HAVE_POPPLER_0_37 1)
set (HAVE_POPPLER_0_50 1)
set (HAVE_POPPLER_0_53 1)
set (HAVE_POPPLER_0_64 1)

set(CMAKE_REQUIRED_INCLUDES ${POPPLER_QT4_INCLUDE_DIR} ${QT_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${POPPLER_QT4_LIBRARIES} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTXML_LIBRARY})

configure_file(
   ${CMAKE_CURRENT_SOURCE_DIR}/config-okular-poppler.h.cmake
   ${CMAKE_CURRENT_BINARY_DIR}/config-okular-poppler.h
)

# Can be multiple include directories for poppler
separate_arguments(POPPLER_QT4_INCLUDE_DIR)
foreach(_include_dir ${POPPLER_QT4_INCLUDE_DIR})
    include_directories(${_include_dir})
endforeach(_include_dir ${POPPLER_QT4_INCLUDE_DIR})

include_directories(
   ${CMAKE_BINARY_DIR}/okular
   ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

########### next target ###############

set(okularGenerator_poppler_PART_SRCS
   generator_pdf.cpp
   formfields.cpp
   annots.cpp
)

kde4_add_ui_files(okularGenerator_poppler_PART_SRCS
   conf/pdfsettingswidget.ui
)

kde4_add_kcfg_files(okularGenerator_poppler_PART_SRCS conf/pdfsettings.kcfgc )

kde4_add_plugin(okularGenerator_poppler ${okularGenerator_poppler_PART_SRCS})

target_link_libraries(okularGenerator_poppler ${POPPLER_QT4_LIBRARIES} okularcore ${KDE4_KDEUI_LIBS} Qt4::QtXml)

install(TARGETS okularGenerator_poppler DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES libokularGenerator_poppler.desktop okularPoppler.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( PROGRAMS okularApplication_pdf.desktop active-documentviewer_pdf.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )

