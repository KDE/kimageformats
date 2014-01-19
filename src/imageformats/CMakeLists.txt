# NB: the desktop files are installed for the benefit of KImageIO in KDE4Support.

# Required for OpenEXR
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE_ENABLE_EXCEPTIONS}")

##################################

add_library(kimg_dds MODULE dds.cpp)
target_link_libraries(kimg_dds Qt5::Gui)

install(TARGETS kimg_dds  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES dds.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

# EPS support depends on the gs utility; non-UNIX systems are unlikely to have
# this available in PATH
if (UNIX)
    find_package(Qt5PrintSupport 5.2.0 NO_MODULE)
    set_package_properties(Qt5PrintSupport PROPERTIES
        PURPOSE "Required for the QImage plugin for EPS images"
        TYPE OPTIONAL
    )

    if (Qt5PrintSupport_FOUND)
        add_library(kimg_eps MODULE eps.cpp)
        target_link_libraries(kimg_eps  Qt5::Gui Qt5::PrintSupport)

        install(TARGETS kimg_eps  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
        install(FILES eps.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)
    endif()
endif()

##################################

find_package(Jasper)
set_package_properties(Jasper PROPERTIES
    DESCRIPTION "A library for handling JPEG-2000 images"
    PURPOSE "Required for the QImage plugin for JPEG-2000 images"
    URL "http://www.ece.uvic.ca/~mdadams/jasper"
    TYPE OPTIONAL
)

check_include_files(sys/types.h   HAVE_SYS_TYPES_H)
check_include_files(stdint.h      HAVE_STDINT_H)
configure_file(config-jp2.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-jp2.h)

if(JASPER_FOUND)
    add_library(kimg_jp2 MODULE jp2.cpp)
    target_compile_options(kimg_jp2 PRIVATE ${JASPER_DEFINITIONS})
    target_include_directories(kimg_jp2 PRIVATE ${JASPER_INCLUDE_DIR})
    target_link_libraries(kimg_jp2  Qt5::Gui ${JASPER_LIBRARIES})

    install(TARGETS kimg_jp2  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
    install(FILES jp2.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)
endif()

##################################

find_package(OpenEXR)
set_package_properties(OpenEXR PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for OpenEXR images"
)

if(OpenEXR_FOUND)
    add_library(kimg_exr MODULE exr.cpp)
    target_link_libraries(kimg_exr Qt5::Gui OpenEXR::IlmImf)

    install(TARGETS kimg_exr  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
    install(FILES exr.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)
endif()

##################################

add_library(kimg_pcx MODULE pcx.cpp)
target_link_libraries(kimg_pcx  Qt5::Gui)

install(TARGETS kimg_pcx  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES pcx.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_pic MODULE pic_read.cpp pic_write.cpp pic.cpp)
target_link_libraries(kimg_pic Qt5::Gui)

install(TARGETS kimg_pic  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES pic.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_psd MODULE psd.cpp)
target_link_libraries(kimg_psd Qt5::Gui)

install(TARGETS kimg_psd  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES psd.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_ras MODULE ras.cpp)
target_link_libraries(kimg_ras  Qt5::Gui)

install(TARGETS kimg_ras  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES ras.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_rgb MODULE rgb.cpp)
target_link_libraries(kimg_rgb  Qt5::Gui)

install(TARGETS kimg_rgb  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES rgb.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_tga MODULE tga.cpp)
target_link_libraries(kimg_tga  Qt5::Gui)

install(TARGETS kimg_tga  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES tga.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_xcf MODULE xcf.cpp)
target_link_libraries(kimg_xcf Qt5::Gui)

install(TARGETS kimg_xcf  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES xcf.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

##################################

add_library(kimg_xview MODULE xview.cpp)
target_link_libraries(kimg_xview Qt5::Gui)

install(TARGETS kimg_xview  DESTINATION ${QT_PLUGIN_INSTALL_DIR}/imageformats/)
install(FILES xv.desktop DESTINATION ${SERVICES_INSTALL_DIR}/qimageioplugins/)

