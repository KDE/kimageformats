#find_package(Qt5Test ${REQUIRED_QT_VERSION} NO_MODULE)

include(ECMMarkAsTest)

add_definitions(-DPLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/../src")
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

macro(kimageformats_read_tests)
    if (NOT TARGET readtest)
        add_executable(readtest readtest.cpp)
        target_link_libraries(readtest Qt5::Gui)
        target_compile_definitions(readtest
            PRIVATE IMAGEDIR="${CMAKE_CURRENT_SOURCE_DIR}/read")
        ecm_mark_as_test(readtest)
    endif()
    foreach(_testname ${ARGN})
        add_test(
            NAME kimageformats-read-${_testname}
            COMMAND readtest ${_testname}
        )
    endforeach(_testname)
endmacro()

macro(kimageformats_write_tests)
    if (NOT TARGET writetest)
        add_executable(writetest writetest.cpp)
        target_link_libraries(writetest Qt5::Gui)
        target_compile_definitions(writetest
            PRIVATE IMAGEDIR="${CMAKE_CURRENT_SOURCE_DIR}/write")
        ecm_mark_as_test(writetest)
    endif()
    foreach(_testname ${ARGN})
        string(REGEX MATCH "-lossless$" _is_lossless "${_testname}")
        unset(lossless_arg)
        if (_is_lossless)
            set(lossless_arg "--lossless")
            string(REGEX REPLACE "-lossless$" "" _testname "${_testname}")
        endif()
        add_test(
            NAME kimageformats-write-${_testname}
            COMMAND writetest ${lossless_arg} ${_testname}
        )
    endforeach(_testname)
endmacro()

# Basic read tests
# Loads each <format> image in read/<format>/, and compares the
# result against the data read from the corresponding png file
kimageformats_read_tests(
    pcx
    pic
    psd
    ras
    rgb
    tga
    xcf
)

# Basic write tests
# Loads each png image in write/, writes the data out
# as a <format> image, and compares the result against the
# the corresponding <format> file.
# You can append -lossless to the format to indicate that
# reading back the image data will result in an identical image.
kimageformats_write_tests(
    pcx-lossless
    pic-lossless
    rgb-lossless
    tga # fixme: the alpha images appear not to be written properly
)

# EPS read tests depend on the vagaries of GhostScript
# which we cannot even guarantee to find, so disable them for now
#if (BUILD_EPS_PLUGIN)
#    kimageformats_read_tests(eps)
#    kimageformats_write_tests(eps)
#endif()
if (JASPER_FOUND)
    kimageformats_read_tests(jp2)
    kimageformats_write_tests(jp2)
endif()
if (OpenEXR_FOUND)
    # FIXME: OpenEXR tests
endif()
if (WebP_FOUND)
    kimageformats_read_tests(webp)
endif()
