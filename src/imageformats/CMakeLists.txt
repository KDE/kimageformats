# NB: the desktop files are installed for the benefit of KImageIO in KDELibs4Support.

##################################

install(FILES dds-qt.desktop RENAME dds.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

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


# need this for Qt's version of the plugin
install(FILES jp2.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

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

##################################

if (KF5Archive_FOUND)

    add_library(kimg_kra MODULE kra.cpp)
    target_link_libraries(kimg_kra Qt5::Gui KF5::Archive)

    install(TARGETS kimg_kra  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
    install(FILES kra.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

    add_library(kimg_ora MODULE ora.cpp)
    target_link_libraries(kimg_ora Qt5::Gui KF5::Archive)

    install(TARGETS kimg_ora  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats/)
    install(FILES ora.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

endif()
