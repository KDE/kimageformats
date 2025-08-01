# NB: the desktop files are installed for the benefit of KImageIO in KDELibs4Support.

##################################

function(kimageformats_add_plugin plugin)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(KIF_ADD_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT KIF_ADD_PLUGIN_SOURCES)
        message(FATAL_ERROR "kimageformats_add_plugin called without SOURCES parameter")
    endif()

    qt_add_plugin(${plugin} PLUGIN_TYPE imageformats)
    target_sources(${plugin} PRIVATE ${KIF_ADD_PLUGIN_SOURCES})
    set_target_properties(${plugin} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/imageformats)
    target_link_libraries(${plugin} PRIVATE Qt6::Gui)

    if(ANDROID)
        # Plugins should be named with lib prefix on Android
        # Working name: libplugins_imageformats_kimg_avif_armeabi-v7a.so
        # Doesn't work: plugins_imageformats_kimg_avif_armeabi-v7a.so
        if(NOT ${CMAKE_SHARED_LIBRARY_PREFIX} STREQUAL "")
            set_target_properties(${plugin} PROPERTIES PREFIX ${CMAKE_SHARED_LIBRARY_PREFIX})
        endif()
    endif()

    install(TARGETS ${plugin} DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/imageformats)
endfunction()

##################################

kimageformats_add_plugin(kimg_ani SOURCES ani.cpp)

##################################

if (TARGET avif)
    kimageformats_add_plugin(kimg_avif SOURCES avif.cpp microexif.cpp)
    target_link_libraries(kimg_avif PRIVATE "avif")
endif()

##################################

if(KIMAGEFORMATS_DDS)
    kimageformats_add_plugin(kimg_dds SOURCES dds.cpp scanlineconverter.cpp)
endif()

##################################

if (BUILD_EPS_PLUGIN)
    if (TARGET Qt6::PrintSupport)
        kimageformats_add_plugin(kimg_eps SOURCES eps.cpp)
        target_link_libraries(kimg_eps PRIVATE Qt6::PrintSupport)
    endif()
endif()

##################################

if(OpenEXR_FOUND)
    kimageformats_add_plugin(kimg_exr SOURCES exr.cpp scanlineconverter.cpp)
    if(TARGET OpenEXR::OpenEXR)
        target_link_libraries(kimg_exr PRIVATE OpenEXR::OpenEXR)
    else()
        if(OpenEXR_VERSION_STRING VERSION_LESS 2.3.0)
            # Older OpenEXR versions use dynamic exception specifications, so
            # cannot use C++17 with them
            set_target_properties(kimg_exr PROPERTIES CXX_STANDARD 14)
        endif()
        target_link_libraries(kimg_exr PRIVATE OpenEXR::IlmImf)
    endif()
    kde_target_enable_exceptions(kimg_exr PRIVATE)
endif()

##################################

kimageformats_add_plugin(kimg_hdr SOURCES hdr.cpp)

##################################

if (LibHeif_FOUND)
    kimageformats_add_plugin(kimg_heif SOURCES heif.cpp microexif.cpp)
    target_link_libraries(kimg_heif PRIVATE PkgConfig::LibHeif)
endif()

##################################

kimageformats_add_plugin(kimg_iff SOURCES iff.cpp chunks.cpp microexif.cpp)

##################################

if (LibJXL_FOUND AND LibJXLThreads_FOUND AND LibJXLCMS_FOUND)
    kimageformats_add_plugin(kimg_jxl SOURCES jxl.cpp microexif.cpp)
    target_link_libraries(kimg_jxl PRIVATE PkgConfig::LibJXL PkgConfig::LibJXLThreads PkgConfig::LibJXLCMS)
endif()

if (OpenJPEG_FOUND)
    kimageformats_add_plugin(kimg_jp2 SOURCES jp2.cpp scanlineconverter.cpp)
    target_include_directories(kimg_jp2 PRIVATE ${OPENJPEG_INCLUDE_DIRS})
    target_link_libraries(kimg_jp2 PRIVATE ${OPENJPEG_LIBRARIES})
endif()

##################################

kimageformats_add_plugin(kimg_pcx SOURCES pcx.cpp)

##################################

kimageformats_add_plugin(kimg_pic SOURCES pic.cpp)

##################################

kimageformats_add_plugin(kimg_pfm SOURCES pfm.cpp)

##################################

kimageformats_add_plugin(kimg_psd SOURCES psd.cpp microexif.cpp scanlineconverter.cpp)

##################################

kimageformats_add_plugin(kimg_pxr SOURCES pxr.cpp)

##################################

kimageformats_add_plugin(kimg_qoi SOURCES qoi.cpp scanlineconverter.cpp)

##################################

kimageformats_add_plugin(kimg_ras SOURCES ras.cpp)

##################################

kimageformats_add_plugin(kimg_rgb SOURCES rgb.cpp)

##################################

kimageformats_add_plugin(kimg_sct SOURCES sct.cpp)

##################################

kimageformats_add_plugin(kimg_tga SOURCES tga.cpp)

##################################

kimageformats_add_plugin(kimg_xcf SOURCES xcf.cpp)

##################################

if (LibRaw_FOUND)
    kimageformats_add_plugin(kimg_raw SOURCES raw.cpp)
    kde_enable_exceptions()
    target_link_libraries(kimg_raw PRIVATE LibRaw::LibRaw)
endif()

##################################

if (LibJXR_FOUND)
    kimageformats_add_plugin(kimg_jxr SOURCES jxr.cpp microexif.cpp)
    kde_enable_exceptions()
    target_include_directories(kimg_jxr PRIVATE ${LIBJXR_INCLUDE_DIRS})
    target_link_libraries(kimg_jxr PRIVATE ${LIBJXR_LIBRARIES})
    target_compile_definitions(kimg_jxr PRIVATE INITGUID)
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=undef")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=undef")
    endif()
endif()

##################################

if (KF6Archive_FOUND)

    kimageformats_add_plugin(kimg_kra SOURCES kra.cpp)
    target_link_libraries(kimg_kra PRIVATE KF6::Archive)

    kimageformats_add_plugin(kimg_ora SOURCES ora.cpp)
    target_link_libraries(kimg_ora PRIVATE KF6::Archive)

endif()
