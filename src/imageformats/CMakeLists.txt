# NB: the desktop files are installed for the benefit of KImageIO in KDELibs4Support.

##################################

if (Qt5Gui_VERSION VERSION_LESS 5.3.0)
    add_library(kimg_dds MODULE dds.cpp)
    target_link_libraries(kimg_dds Qt5::Gui)

    install(TARGETS kimg_dds  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
    install(FILES dds.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
else()
    install(FILES dds-qt.desktop RENAME dds.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
endif()

##################################

if (BUILD_EPS_PLUGIN)
    if (Qt5PrintSupport_FOUND)
        add_library(kimg_eps MODULE eps.cpp)
        target_link_libraries(kimg_eps  Qt5::Gui Qt5::PrintSupport)

        install(TARGETS kimg_eps  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
        install(FILES eps.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
    endif()
endif()

##################################

check_include_files(sys/types.h   HAVE_SYS_TYPES_H)
check_include_files(stdint.h      HAVE_STDINT_H)
configure_file(config-jp2.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-jp2.h)

if(JASPER_FOUND)
    add_library(kimg_jp2 MODULE jp2.cpp)
    target_compile_options(kimg_jp2 PRIVATE ${JASPER_DEFINITIONS})
    target_include_directories(kimg_jp2 PRIVATE ${JASPER_INCLUDE_DIR})
    target_link_libraries(kimg_jp2  Qt5::Gui ${JASPER_LIBRARIES})

    install(TARGETS kimg_jp2  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
    install(FILES jp2.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
elseif (NOT Qt5Gui_VERSION VERSION_LESS 5.3.0)
    # need this for Qt's version of the plugin
    install(FILES jp2.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
endif()

##################################

if(OpenEXR_FOUND)
    add_library(kimg_exr MODULE exr.cpp)
    target_link_libraries(kimg_exr Qt5::Gui OpenEXR::IlmImf)
    kde_target_enable_exceptions(kimg_exr PRIVATE)

    install(TARGETS kimg_exr  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
    install(FILES exr.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
endif()

##################################

add_library(kimg_pcx MODULE pcx.cpp)
target_link_libraries(kimg_pcx  Qt5::Gui)

install(TARGETS kimg_pcx  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES pcx.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

add_library(kimg_pic MODULE pic.cpp)
target_link_libraries(kimg_pic Qt5::Gui)

install(TARGETS kimg_pic  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES pic.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

add_library(kimg_psd MODULE psd.cpp)
target_link_libraries(kimg_psd Qt5::Gui)

install(TARGETS kimg_psd  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES psd.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

add_library(kimg_ras MODULE ras.cpp)
target_link_libraries(kimg_ras  Qt5::Gui)

install(TARGETS kimg_ras  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES ras.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

add_library(kimg_rgb MODULE rgb.cpp)
target_link_libraries(kimg_rgb  Qt5::Gui)

install(TARGETS kimg_rgb  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES rgb.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

add_library(kimg_tga MODULE tga.cpp)
target_link_libraries(kimg_tga  Qt5::Gui)

install(TARGETS kimg_tga  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES tga.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

add_library(kimg_xcf MODULE xcf.cpp)
target_link_libraries(kimg_xcf Qt5::Gui)

install(TARGETS kimg_xcf  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
install(FILES xcf.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

