cmake_minimum_required(VERSION 2.8.12)

project(KImageFormats)

find_package(ECM 0.0.11 REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings)
include(KDECMakeSettings)

include(FeatureSummary)
include(CheckIncludeFiles)

set(REQUIRED_QT_VERSION 5.2.0)
find_package(Qt5Gui ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

# EPS support depends on the gs utility; non-UNIX systems are unlikely to have
# this available in PATH
set(BUILD_EPS_PLUGIN FALSE)
if (UNIX)
    find_package(Qt5PrintSupport 5.2.0 NO_MODULE)
    set_package_properties(Qt5PrintSupport PROPERTIES
        PURPOSE "Required for the QImage plugin for EPS images"
        TYPE OPTIONAL
    )
    if (Qt5PrintSupport_FOUND)
        set(BUILD_EPS_PLUGIN TRUE)
    endif()
endif()

find_package(Jasper)
set_package_properties(Jasper PROPERTIES
    DESCRIPTION "A library for handling JPEG-2000 images"
    PURPOSE "Required for the QImage plugin for JPEG-2000 images"
    URL "http://www.ece.uvic.ca/~mdadams/jasper"
    TYPE OPTIONAL
)

find_package(OpenEXR)
set_package_properties(OpenEXR PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for OpenEXR images"
)

add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
