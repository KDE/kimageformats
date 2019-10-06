cmake_minimum_required(VERSION 3.5)

project(KImageFormats)

include(FeatureSummary)
find_package(ECM 5.63.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://projects.kde.org/projects/kdesupport/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)


set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)


include(CheckIncludeFiles)

set(REQUIRED_QT_VERSION 5.11.0)
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
add_definitions(-DQT_NO_FOREACH)
add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
