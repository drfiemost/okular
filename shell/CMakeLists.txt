
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# okular

set(okular_SRCS
   main.cpp
   okular_main.cpp
   shell.cpp
   shellutils.cpp
)

kde4_add_app_icon(okular_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../ui/data/icons/hi*-apps-okular.png")

kde4_add_executable(okular ${okular_SRCS})

target_link_libraries(okular ${KDE4_KPARTS_LIBS})

install(TARGETS okular ${INSTALL_TARGETS_DEFAULT_ARGS})


# okular shell data files

install( PROGRAMS okular.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES shell.rc DESTINATION ${DATA_INSTALL_DIR}/okular )

