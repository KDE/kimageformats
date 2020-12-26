#find_package(Qt5Test ${REQUIRED_QT_VERSION} NO_MODULE)

include(ECMMarkAsTest)
include(CMakeParseArguments)

add_definitions(-DPLUGIN_DIR="${CMAKE_CURRENT_BINARY_DIR}/../bin")
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

macro(kimageformats_read_tests)
    cmake_parse_arguments(KIF_RT "" "FUZZ" "" ${ARGN})
    set(_fuzzarg)
    if (KIF_RT_FUZZ)
        set(_fuzzarg -f ${KIF_RT_FUZZ})
    endif()

    if (NOT TARGET readtest)
        add_executable(readtest readtest.cpp)
        target_link_libraries(readtest Qt5::Gui)
        target_compile_definitions(readtest
            PRIVATE IMAGEDIR="${CMAKE_CURRENT_SOURCE_DIR}/read")
        ecm_mark_as_test(readtest)
    endif()

    foreach(_testname ${KIF_RT_UNPARSED_ARGUMENTS})
        add_test(
            NAME kimageformats-read-${_testname}
            COMMAND readtest ${_fuzzarg} ${_testname}
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
    hdr
    pcx
    psd
    ras
    rgb
    tga
)

if (KF5Archive_FOUND)
    kimageformats_read_tests(
        kra
        ora
    )
endif()

if (TARGET avif)
    kimageformats_read_tests(
        avif
    )
endif()

# Allow some fuzziness when reading this formats, to allow for
# rounding errors (eg: in alpha blending).
kimageformats_read_tests(FUZZ 1
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
if (OpenEXR_FOUND)
    # FIXME: OpenEXR tests
endif()

find_package(Qt5Test ${REQUIRED_QT_VERSION} CONFIG QUIET)

if(NOT Qt5Test_FOUND)
    message(STATUS "Qt5Test not found, some autotests will not be built.")
    return()
endif()

add_executable(pictest pictest.cpp)
target_link_libraries(pictest Qt5::Gui Qt5::Test)
ecm_mark_as_test(pictest)
add_test(NAME kimageformats-pic COMMAND pictest)

add_executable(anitest anitest.cpp)
target_link_libraries(anitest Qt5::Gui Qt5::Test)
ecm_mark_as_test(anitest)
add_test(NAME kimageformats-ani COMMAND anitest)
