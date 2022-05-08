set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

kde4_add_unit_test( shelltest shelltest.cpp ../shell/shellutils.cpp )
target_link_libraries( shelltest ${KDE4_KDECORE_LIBS} Qt4::QtTest )

kde4_add_unit_test( parttest parttest.cpp )
target_link_libraries( parttest ${KDE4_KDECORE_LIBS} ${KDE4_KPARTS_LIBS} Qt4::QtGui Qt4::QtTest okularpart okularcore )

kde4_add_unit_test( documenttest documenttest.cpp )
target_link_libraries( documenttest ${KDE4_KDECORE_LIBS} ${KDE4_THREADWEAVER_LIBRARY} Qt4::QtGui Qt4::QtTest okularcore )

kde4_add_unit_test( searchtest searchtest.cpp )
target_link_libraries( searchtest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest okularcore )

kde4_add_unit_test( annotationstest annotationstest.cpp )
target_link_libraries( annotationstest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest okularcore )

kde4_add_unit_test( urldetecttest urldetecttest.cpp )
target_link_libraries( urldetecttest ${KDE4_KDECORE_LIBS} Qt4::QtTest )

kde4_add_unit_test( editannotationcontentstest editannotationcontentstest.cpp testingutils.cpp)
target_link_libraries( editannotationcontentstest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest Qt4::QtXml okularcore )

kde4_add_unit_test( addremoveannotationtest addremoveannotationtest.cpp testingutils.cpp)
target_link_libraries( addremoveannotationtest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest Qt4::QtXml okularcore )

kde4_add_unit_test( translateannotationtest translateannotationtest.cpp testingutils.cpp)
target_link_libraries( translateannotationtest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest Qt4::QtXml okularcore )

kde4_add_unit_test( modifyannotationpropertiestest modifyannotationpropertiestest.cpp testingutils.cpp)
target_link_libraries( modifyannotationpropertiestest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest Qt4::QtXml okularcore )

kde4_add_unit_test( editformstest editformstest.cpp )
target_link_libraries( editformstest ${KDE4_KDECORE_LIBS} Qt4::QtGui Qt4::QtTest Qt4::QtXml okularcore )

kde4_add_unit_test( mainshelltest mainshelltest.cpp ../shell/okular_main.cpp ../shell/shellutils.cpp ../shell/shell.cpp )
target_link_libraries( mainshelltest ${KDE4_KPARTS_LIBS} Qt4::QtTest okularpart okularcore )
