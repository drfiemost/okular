cmake_minimum_required(VERSION 3.5)
project(okular)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
        "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options("$<$<CONFIG:DEBUG>:-Wall;-Wextra;-pedantic>")
endif()

find_package(KDE4 4.6.0 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(MacroOptionalAddSubdirectory)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

macro_optional_find_package(QImageBlitz)
macro_log_feature(QIMAGEBLITZ_FOUND "QImageBlitz" "An image effects library" "http://sourceforge.net/projects/qimageblitz" TRUE "kdesupport" "Required to build Okular.")

macro_optional_find_package(LibKScreen)
macro_log_feature(LibKScreen_FOUND "LibKScreen" "KDE screen management library" "https://projects.kde.org/projects/kdereview/libkscreen" FALSE "1.0.2" "DPI detection support")

find_package(ZLIB REQUIRED)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${KDE4_INCLUDES}
   ${QIMAGEBLITZ_INCLUDES}
   ${LibKScreen_INCLUDE_DIR}
   ${ZLIB_INCLUDE_DIR}
   core/synctex
)

option(BUILD_COVERAGE "Build the project with gcov support" OFF)

if(BUILD_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
endif()

add_subdirectory( active )
add_subdirectory( ui )
add_subdirectory( shell )
add_subdirectory( generators )
macro_optional_add_subdirectory( tests )
macro_optional_add_subdirectory(doc)

include(OkularConfigureChecks.cmake)
include(MacroWriteBasicCMakeVersionFile)

if(NOT WIN32)
 set(MATH_LIB m)
else(NOT WIN32)
 set(MATH_LIB)
endif(NOT WIN32)

set(CMAKE_AUTOMOC ON)
set_property(SOURCE ui_textdocumentsettings.h
                    ui_chooseenginewidget.h
                    ui_dlgaccessibilitybase.h
                    ui_dlgeditorbase.h
                    ui_dlggeneralbase.h
                    ui_dlgannotationsbase.h
                    ui_dlgperformancebase.h
                    ui_dlgpresentationbase.h
                    settings.cpp
                    settings.h
                    kspeechinterface.h
                    settings_core.h
                    settings_core.cpp PROPERTY SKIP_AUTOMOC ON)

# okularcore

set(okularcore_SRCS
   core/qpagesize.cpp # REMOVE On Qt5 port
   core/action.cpp
   core/annotations.cpp
   core/area.cpp
   core/audioplayer.cpp
   core/bookmarkmanager.cpp
   core/chooseenginedialog.cpp
   core/document.cpp
   core/documentcommands.cpp
   core/fontinfo.cpp
   core/form.cpp
   core/generator.cpp
   core/generator_p.cpp
   core/misc.cpp
   core/movie.cpp
   core/observer.cpp
   core/page.cpp
   core/pagecontroller.cpp
   core/pagesize.cpp
   core/pagetransition.cpp
   core/rotationjob.cpp
   core/scripter.cpp
   core/sound.cpp
   core/sourcereference.cpp
   core/textdocumentgenerator.cpp
   core/textdocumentsettings.cpp
   core/textpage.cpp
   core/tilesmanager.cpp
   core/utils.cpp
   core/view.cpp
   core/fileprinter.cpp
   core/script/executor_kjs.cpp
   core/script/kjs_app.cpp
   core/script/kjs_console.cpp
   core/script/kjs_data.cpp
   core/script/kjs_document.cpp
   core/script/kjs_fullscreen.cpp
   core/script/kjs_field.cpp
   core/script/kjs_spell.cpp
   core/script/kjs_util.cpp
   core/synctex/synctex_parser.c
   core/synctex/synctex_parser_utils.c
)

kde4_add_ui_files(okularcore_SRCS
   conf/textdocumentsettings.ui
)

install( FILES
           core/action.h
           core/annotations.h
           core/area.h
           core/document.h
           core/fontinfo.h
           core/form.h
           core/generator.h
           core/global.h
           core/okular_export.h
           core/page.h
           core/pagesize.h
           core/pagetransition.h
           core/sound.h
           core/sourcereference.h
           core/textdocumentgenerator.h
           core/textdocumentsettings.h
           core/textpage.h
           core/tile.h
           core/utils.h
           core/version.h
           core/fileprinter.h
           core/observer.h
           ${CMAKE_CURRENT_BINARY_DIR}/settings_core.h
         DESTINATION ${INCLUDE_INSTALL_DIR}/okular/core COMPONENT Devel)

install( FILES
           interfaces/configinterface.h
           interfaces/guiinterface.h
           interfaces/printinterface.h
           interfaces/saveinterface.h
           interfaces/viewerinterface.h
         DESTINATION ${INCLUDE_INSTALL_DIR}/okular/interfaces COMPONENT Devel)

kde4_add_ui_files(okularcore_SRCS
   core/chooseenginewidget.ui
)

kde4_add_kcfg_files(okularcore_SRCS conf/settings_core.kcfgc )

kde4_add_library(okularcore SHARED ${okularcore_SRCS})

# Special handling for linking okularcore on OSX/Apple
IF(APPLE)
    SET(OKULAR_IOKIT "-framework IOKit" CACHE STRING "Apple IOKit framework")
ENDIF(APPLE)

target_link_libraries(okularcore  ${OKULAR_IOKIT} ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBRARY} ${KDE4_KJSAPI_LIBRARY} ${MATH_LIB} ${KDE4_THREADWEAVER_LIBRARY} ${ZLIB_LIBRARIES} )

if(LibKScreen_FOUND)
    target_link_libraries(okularcore ${LibKScreen_LIBRARY})
endif(LibKScreen_FOUND)

set_target_properties(okularcore PROPERTIES VERSION 7.0.0 SOVERSION 7 )

install(TARGETS okularcore ${INSTALL_TARGETS_DEFAULT_ARGS} )

install(FILES conf/okular.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install(FILES conf/okular_core.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install(FILES core/okularGenerator.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})

# okularpart

set(okularpart_SRCS
   part.cpp
   extensions.cpp
   conf/preferencesdialog.cpp
   conf/dlgaccessibility.cpp
   conf/dlgdebug.cpp
   conf/dlgeditor.cpp
   conf/dlggeneral.cpp
   conf/dlgannotations.cpp
   conf/dlgperformance.cpp
   conf/dlgpresentation.cpp
   conf/widgetannottools.cpp
   ui/embeddedfilesdialog.cpp
   ui/annotwindow.cpp
   ui/annotationmodel.cpp
   ui/annotationpopup.cpp
   ui/annotationpropertiesdialog.cpp
   ui/annotationproxymodels.cpp
   ui/annotationtools.cpp
   ui/annotationwidgets.cpp
   ui/bookmarklist.cpp
   ui/fileprinterpreview.cpp
   ui/findbar.cpp
   ui/formwidgets.cpp
   ui/guiutils.cpp
   ui/ktreeviewsearchline.cpp
   ui/latexrenderer.cpp
   ui/minibar.cpp
   ui/pageitemdelegate.cpp
   ui/pagepainter.cpp
   ui/pagesizelabel.cpp
   ui/pageviewannotator.cpp
   ui/pageview.cpp
   ui/magnifierview.cpp
   ui/pageviewutils.cpp
   ui/presentationsearchbar.cpp
   ui/presentationwidget.cpp
   ui/propertiesdialog.cpp
   ui/searchlineedit.cpp
   ui/searchwidget.cpp
   ui/sidebar.cpp
   ui/side_reviews.cpp
   ui/snapshottaker.cpp
   ui/thumbnaillist.cpp
   ui/toc.cpp
   ui/tocmodel.cpp
   ui/toolaction.cpp
   ui/tts.cpp
   ui/videowidget.cpp
   ui/layers.cpp
)

kde4_add_ui_files(okularpart_SRCS
   conf/dlgaccessibilitybase.ui
   conf/dlgeditorbase.ui
   conf/dlggeneralbase.ui
   conf/dlgannotationsbase.ui
   conf/dlgperformancebase.ui
   conf/dlgpresentationbase.ui
)

kde4_add_kcfg_files(okularpart_SRCS conf/settings.kcfgc )

qt4_add_dbus_interfaces(okularpart_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.kde.KSpeech.xml)

kde4_add_plugin(okularpart SHARED ${okularpart_SRCS})

target_link_libraries(okularpart okularcore ${KDE4_KPARTS_LIBS} ${KDE4_KPRINTUTILS_LIBS} ${MATH_LIB} ${QIMAGEBLITZ_LIBRARIES} ${KDE4_PHONON_LIBRARY} ${KDE4_SOLID_LIBRARY})

install(TARGETS okularpart DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install(FILES okular.upd DESTINATION ${DATA_INSTALL_DIR}/kconf_update)

install( FILES okular_part.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES part.rc part-viewermode.rc DESTINATION ${DATA_INSTALL_DIR}/okular )

########### cmake files #################

macro_write_basic_cmake_version_file(${CMAKE_CURRENT_BINARY_DIR}/OkularConfigVersion.cmake 0 21 0)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/OkularConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/OkularConfigVersion.cmake
        DESTINATION ${LIB_INSTALL_DIR}/cmake/Okular )

macro_display_feature_log()

