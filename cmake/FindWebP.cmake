# Try to find the WebP library
#
# This will define:
#
#   WebP_FOUND        - True if WebP is available
#   WebP_LIBRARIES    - Link to these to use WebP
#   WebP_INCLUDE_DIRS - Include directory for WebP
#   WebP_DEFINITIONS  - Compiler flags required to link against WebP
#
# In addition the following more fine grained variables will be defined:
#
#   WebP_WebP_FOUND        WebP_WebP_INCLUDE_DIR        WebP_WebP_LIBRARY
#   WebP_Decoder_FOUND     WebP_Decoder_INCLUDE_DIR     WebP_Decoder_LIBRARY
#   WebP_DeMux_FOUND       WebP_DeMux_INCLUDE_DIR       WebP_DeMux_LIBRARY
#   WebP_Mux_FOUND         WebP_Mux_INCLUDE_DIR         WebP_Mux_LIBRARY
#
# Additionally, the following imported targets will be defined:
#
#   WebP::WebP
#   WebP::Decoder
#   WebP::DeMux
#   WebP::Mux
#
# Note that the Decoder library provides a strict subset of the functionality
# of the WebP library; it is therefore not included in WebP_LIBRARIES (unless
# the WebP library is not found), and you should not link to both WebP::WebP
# and WebP::Decoder.
#
# Copyright (c) 2011 Fredrik Höglund <fredrik@kde.org>
# Copyright (c) 2013 Martin Gräßlin <mgraesslin@kde.org>
# Copyright (c) 2013-2014, Alex Merry, <alex.merry@kdemail.net>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if(${CMAKE_VERSION} VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindWebP.cmake")
endif()

set(knownComponents
    WebP
    Decoder
    DeMux
    Mux)

unset(unknownComponents)

set(pkgConfigModules)
set(requiredComponents)

if (WebP_FIND_COMPONENTS)
    set(comps ${WebP_FIND_COMPONENTS})
else()
    set(comps ${knownComponents})
endif()

# iterate through the list of requested components, and check that we know them all.
# If not, fail.
foreach(comp ${comps})
    list(FIND knownComponents ${comp} index )
    if("${index}" STREQUAL "-1")
        list(APPEND unknownComponents "${comp}")
    else()
        if("${comp}" STREQUAL "WebP")
            list(APPEND pkgConfigModules "libwebp")
        elseif("${comp}" STREQUAL "Decoder")
            list(APPEND pkgConfigModules "libwebpdecoder")
        elseif("${comp}" STREQUAL "DeMux")
            list(APPEND pkgConfigModules "libwebpdemux")
        elseif("${comp}" STREQUAL "Mux")
            list(APPEND pkgConfigModules "libwebpmux")
        endif()
    endif()
endforeach()

if(DEFINED unknownComponents)
   set(msgType STATUS)
   if(WebP_FIND_REQUIRED)
      set(msgType FATAL_ERROR)
   endif()
   if(NOT WebP_FIND_QUIETLY)
      message(${msgType} "WebP: requested unknown components ${unknownComponents}")
   endif()
   return()
endif()

macro(_WEBP_HANDLE_COMPONENT _comp)
    set(_header)
    set(_lib)
    set(_pkgconfig_module)
    if("${_comp}" STREQUAL "WebP")
        set(_header "webp/encode.h")
        set(_lib "webp")
        set(_pkgconfig_module "libwebp")
    elseif("${_comp}" STREQUAL "Decoder")
        set(_header "webp/decode.h")
        set(_lib "webpdecoder")
        set(_pkgconfig_module "libwebpdecoder")
    elseif("${_comp}" STREQUAL "DeMux")
        set(_header "webp/demux.h")
        set(_lib "webpdemux")
        set(_pkgconfig_module "libwebpdemux")
    elseif("${_comp}" STREQUAL "Mux")
        set(_header "webp/mux.h")
        set(_lib "webpmux")
        set(_pkgconfig_module "libwebpmux")
    endif()

    find_path(WebP_${_comp}_INCLUDE_DIR NAMES ${_header} HINTS ${PKG_WebP_INCLUDE_DIRS})
    find_library(WebP_${_comp}_LIBRARY NAMES ${_lib} HINTS ${PKG_WebP_LIBRARY_DIRS})

    if(WebP_${_comp}_INCLUDE_DIR AND WebP_${_comp}_LIBRARY)
        if(NOT "${_comp}" STREQUAL "Decoder")
            list(APPEND WebP_INCLUDE_DIRS ${WebP_${_comp}_INCLUDE_DIR})
            list(APPEND WebP_LIBRARIES ${WebP_${_comp}_LIBRARY})
        endif()
    endif()

    if(PKG_WebP_VERSION AND NOT PKG_WebP_${_pkgconfig_module}_VERSION)
        # this is what gets set if we only search for one module
        set(WebP_${_comp}_VERSION_STRING "${PKG_WebP_VERSION}")
    else()
        set(WebP_${_comp}_VERSION_STRING "${PKG_WebP_${_pkgconfig_module}_VERSION}")
    endif()
    if(NOT WebP_VERSION_STRING)
        set(WebP_VERSION_STRING ${WebP_${_comp}_VERSION_STRING})
    endif()

    find_package_handle_standard_args(WebP_${_comp}
        FOUND_VAR
            WebP_${_comp}_FOUND
        REQUIRED_VARS
            WebP_${_comp}_LIBRARY
            WebP_${_comp}_INCLUDE_DIR
        VERSION_VAR
            WebP_${_comp}_VERSION_STRING
        )

    mark_as_advanced(WebP_${_comp}_LIBRARY WebP_${_comp}_INCLUDE_DIR)

    # compatibility for old variable naming
    set(WebP_${_comp}_INCLUDE_DIRS ${WebP_${_comp}_INCLUDE_DIR})
    set(WebP_${_comp}_LIBRARIES ${WebP_${_comp}_LIBRARY})

    if(WebP_${_comp}_FOUND AND NOT TARGET WebP::${_comp})
        add_library(WebP::${_comp} UNKNOWN IMPORTED)
        set_target_properties(WebP::${_comp} PROPERTIES
            IMPORTED_LOCATION "${WebP_${_comp}_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${WebP_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${WebP_${_comp}_INCLUDE_DIR}"
        )
    endif()
endmacro()

include(FindPackageHandleStandardArgs)
# Use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig)
pkg_check_modules(PKG_WebP QUIET ${pkgConfigModules})

set(WebP_DEFINITIONS ${PKG_WebP_CFLAGS})

foreach(comp ${comps})
    _webp_handle_component(${comp})
endforeach()

if (WebP_Decoder_FOUND AND NOT WebP_WebP_FOUND)
    list(APPEND WebP_INCLUDE_DIRS ${WebP_Decoder_INCLUDE_DIR})
    list(APPEND WebP_LIBRARIES ${WebP_Decoder_LIBRARY})
endif()

if (WebP_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES WebP_INCLUDE_DIRS)
endif()

if(WebP_WebP_FOUND)
    if(WebP_DeMux_FOUND)
        add_dependencies(WebP::DeMux WebP::WebP)
    endif()
    if(WebP_Mux_FOUND)
        add_dependencies(WebP::Mux WebP::WebP)
    endif()
endif()

find_package_handle_standard_args(WebP
    FOUND_VAR
        WebP_FOUND
    REQUIRED_VARS
        WebP_LIBRARIES
        WebP_INCLUDE_DIRS
    VERSION_VAR
        WebP_VERSION_STRING
    HANDLE_COMPONENTS
    )

# compatibility for old variable naming
set(WebP_INCLUDE_DIR ${WebP_INCLUDE_DIRS})

include(FeatureSummary)
set_package_properties(WebP PROPERTIES
   URL http://www.openexr.com/
   DESCRIPTION "A library for handling WebP/VP8 image files")

