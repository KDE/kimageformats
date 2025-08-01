cmake_minimum_required(VERSION 3.16)

set(KF_VERSION "6.18.0") # handled by release scripts
set(KF_DEP_VERSION "6.17.0") # handled by release scripts
project(KImageFormats VERSION ${KF_VERSION})

include(FeatureSummary)
find_package(ECM 6.17.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://commits.kde.org/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)


set(CMAKE_MODULE_PATH ${KImageFormats_SOURCE_DIR}/cmake/find-modules ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

include(KDEGitCommitHooks)
include(ECMDeprecationSettings)
include(CheckIncludeFiles)
include(FindPkgConfig)

set(REQUIRED_QT_VERSION 6.7.0)
find_package(Qt6Gui ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

find_package(KF6Archive ${KF_DEP_VERSION})
set_package_properties(KF6Archive PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for Krita and OpenRaster images"
)

# EPS support depends on the gs utility; non-UNIX systems are unlikely to have
# this available in PATH
set(BUILD_EPS_PLUGIN FALSE)
if (UNIX)
    find_package(Qt6PrintSupport ${REQUIRED_QT_VERSION} NO_MODULE)
    set_package_properties(Qt6PrintSupport PROPERTIES
        PURPOSE "Required for the QImage plugin for EPS images"
        TYPE OPTIONAL
    )
    if (TARGET Qt6::PrintSupport)
        set(BUILD_EPS_PLUGIN TRUE)
    endif()
endif()

find_package(OpenEXR 3.0 CONFIG QUIET)
if(NOT OpenEXR_FOUND)
    find_package(OpenEXR)
endif()
set_package_properties(OpenEXR PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for OpenEXR images"
)

find_package(libavif 0.8.2 CONFIG QUIET)
if(NOT libavif_FOUND)
    find_package(libavif 1 CONFIG)
endif()
set_package_properties(libavif PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for AVIF images"
)

option(KIMAGEFORMATS_DDS "Enable plugin for DDS format" ON)

option(KIMAGEFORMATS_HEIF "Enable plugin for HEIF format" OFF)
if(KIMAGEFORMATS_HEIF)
    pkg_check_modules(LibHeif IMPORTED_TARGET libheif>=1.10.0)
endif()
add_feature_info(LibHeif LibHeif_FOUND "required for the QImage plugin for HEIF/HEIC images")

option(KIMAGEFORMATS_JXL "Enable plugin for JPEG XL format" ON)
if(KIMAGEFORMATS_JXL)
    pkg_check_modules(LibJXL IMPORTED_TARGET libjxl>=0.9.4)
    pkg_check_modules(LibJXLThreads IMPORTED_TARGET libjxl_threads>=0.9.4)
    pkg_check_modules(LibJXLCMS IMPORTED_TARGET libjxl_cms>=0.9.4)
endif()
add_feature_info(LibJXL LibJXL_FOUND "required for the QImage plugin for JPEG XL images")

option(KIMAGEFORMATS_JP2 "Enable plugin for JPEG 2000 format" ON)
if(KIMAGEFORMATS_JP2)
    find_package(OpenJPEG CONFIG)
endif()
add_feature_info(OpenJPEG OpenJPEG_FOUND "required for the QImage plugin for JPEG 2000 images")


find_package(LibRaw 0.20.2)
set_package_properties(LibRaw PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for the QImage plugin for RAW images"
)

# JXR plugin disabled by default due to security issues
option(KIMAGEFORMATS_JXR "Enable plugin for JPEG XR format" OFF)
if(KIMAGEFORMATS_JXR)
    find_package(LibJXR)
endif()
add_feature_info(LibJXR LibJXR_FOUND "required for the QImage plugin for JPEG XR images")

ecm_set_disabled_deprecation_versions(
    QT 6.10.0
    KF 6.16.0
)

add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF6ImageFormats")
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/KF6ImageFormatsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/KF6ImageFormatsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KF6ImageFormatsConfig.cmake"
    INSTALL_DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF6ImageFormatsConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF6ImageFormatsConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

include(ECMFeatureSummary)
ecm_feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
