cmake_minimum_required(VERSION 3.6)

project(KImageFormats)

set (CMAKE_CXX_STANDARD 14)

include(FeatureSummary)
find_package(ECM 5.80.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://commits.kde.org/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)


set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)


include(CheckIncludeFiles)

set(REQUIRED_QT_VERSION 5.14.0)
find_package(Qt5Gui ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

find_package(KF5Archive)
set_package_properties(KF5Archive PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for Krita and OpenRaster images"
)

# EPS support depends on the gs utility; non-UNIX systems are unlikely to have
# this available in PATH
set(BUILD_EPS_PLUGIN FALSE)
if (UNIX)
    find_package(Qt5PrintSupport ${REQUIRED_QT_VERSION} NO_MODULE)
    set_package_properties(Qt5PrintSupport PROPERTIES
        PURPOSE "Required for the QImage plugin for EPS images"
        TYPE OPTIONAL
    )
    if (Qt5PrintSupport_FOUND)
        set(BUILD_EPS_PLUGIN TRUE)
    endif()
endif()

find_package(OpenEXR)
set_package_properties(OpenEXR PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for OpenEXR images"
)

find_package(libavif 0.8.2 CONFIG)
set_package_properties(libavif PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for AVIF images"
)

option(KIMAGEFORMATS_HEIF "Enable plugin for HEIF format" OFF)
if(KIMAGEFORMATS_HEIF)
    include(FindPkgConfig)
    pkg_check_modules(LibHeif IMPORTED_TARGET libheif>=1.10.0)
endif()
add_feature_info(LibHeif LibHeif_FOUND "required for the QImage plugin for HEIF/HEIC images")

add_definitions(-DQT_NO_FOREACH)
# 050d00 (5.13) triggers a BIC in qimageiohandler.h, in Qt 5.13, so do not enable that until we can require 5.14
# https://codereview.qt-project.org/c/qt/qtbase/+/279215
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050e00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054F00)
add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
