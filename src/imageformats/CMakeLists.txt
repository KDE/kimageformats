# NB: the desktop files are installed for the benefit of KImageIO in KDELibs4Support.

##################################

function(kimageformats_add_plugin plugin)
    set(options)
    set(oneValueArgs JSON)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(KIF_ADD_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT KIF_ADD_PLUGIN_SOURCES)
        message(FATAL_ERROR "kimageformats_add_plugin called without SOURCES parameter")
    endif()
    get_filename_component(json "${KIF_ADD_PLUGIN_JSON}" REALPATH)
    if(NOT KIF_ADD_PLUGIN_JSON OR NOT EXISTS ${json})
        message(FATAL_ERROR "JSON file doesn't exist: ${json}")
    endif()

    add_library(${plugin} MODULE ${KIF_ADD_PLUGIN_SOURCES})
    set_property(TARGET ${plugin} APPEND PROPERTY AUTOGEN_TARGET_DEPENDS ${json})
    set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/imageformats")
    target_link_libraries(${plugin} Qt5::Gui)
    install(TARGETS ${plugin} DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats)
endfunction()

##################################

install(FILES dds-qt.desktop RENAME dds.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

if (BUILD_EPS_PLUGIN)
    if (Qt5PrintSupport_FOUND)
        kimageformats_add_plugin(kimg_eps JSON "eps.json" SOURCES eps.cpp)
        target_link_libraries(kimg_eps Qt5::PrintSupport)
        install(FILES eps.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
    endif()
endif()

##################################

# need this for Qt's version of the plugin
install(FILES jp2.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

if(OpenEXR_FOUND)
    kimageformats_add_plugin(kimg_exr JSON "exr.json" SOURCES exr.cpp)
    target_link_libraries(kimg_exr OpenEXR::IlmImf)
    kde_target_enable_exceptions(kimg_exr PRIVATE)

    install(FILES exr.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)
endif()

##################################

kimageformats_add_plugin(kimg_pcx JSON "pcx.json" SOURCES pcx.cpp)
install(FILES pcx.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

kimageformats_add_plugin(kimg_pic JSON "pic.json" SOURCES pic.cpp)
install(FILES pic.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

kimageformats_add_plugin(kimg_psd JSON "psd.json" SOURCES psd.cpp)
install(FILES psd.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

kimageformats_add_plugin(kimg_ras JSON "ras.json" SOURCES ras.cpp)
install(FILES ras.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

kimageformats_add_plugin(kimg_rgb JSON "rgb.json" SOURCES rgb.cpp)
install(FILES rgb.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

kimageformats_add_plugin(kimg_tga JSON "tga.json" SOURCES tga.cpp)
install(FILES tga.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

kimageformats_add_plugin(kimg_xcf JSON "xcf.json" SOURCES xcf.cpp)
install(FILES xcf.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

##################################

if (KF5Archive_FOUND)

    kimageformats_add_plugin(kimg_kra JSON "kra.json" SOURCES kra.cpp)
    target_link_libraries(kimg_kra KF5::Archive)
    install(FILES kra.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

    kimageformats_add_plugin(kimg_ora JSON "ora.json" SOURCES ora.cpp)
    target_link_libraries(kimg_ora KF5::Archive)
    install(FILES ora.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/qimageioplugins/)

endif()
