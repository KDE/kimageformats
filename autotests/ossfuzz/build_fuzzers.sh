#!/bin/bash -eu
#
# SPDX-FileCopyrightText: 2020 Google LLC
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
LDFLAGS=""
if [[ $FUZZING_ENGINE == "afl" ]]; then
    LDFLAGS="-fuse-ld=lld"
fi
export LDFLAGS

# build zstd
cd $SRC/zstd
cmake -G Ninja -S build/cmake -DBUILD_SHARED_LIBS=OFF -DZSTD_BUILD_PROGRAMS=OFF -DBUILD_TESTING=OFF
ninja install

# Build zlib
cd $SRC/zlib
./configure --static
make install -j$(nproc)

# Build bzip2
# Inspired from ../bzip2/build
cd $SRC
tar xzf bzip2-*.tar.gz && rm -f bzip2-*.tar.gz
cd bzip2-*
SRCL=(blocksort.o huffman.o crctable.o randtable.o compress.o decompress.o bzlib.o)

for source in ${SRCL[@]}; do
    name=$(basename $source .o)
    $CC $CFLAGS -c ${name}.c
done
rm -f libbz2.a
ar cq libbz2.a ${SRCL[@]}
cp -f bzlib.h /usr/local/include
cp -f libbz2.a /usr/local/lib

# Build xz
export ORIG_CFLAGS="${CFLAGS}"
export ORIG_CXXFLAGS="${CXXFLAGS}"
unset CFLAGS
unset CXXFLAGS
cd $SRC/xz
./autogen.sh --no-po4a --no-doxygen
./configure --enable-static --disable-debug --disable-shared --disable-xz --disable-xzdec --disable-lzmainfo
make install -j$(nproc)
export CFLAGS="${ORIG_CFLAGS}"
export CXXFLAGS="${ORIG_CXXFLAGS}"

# Build qt
cd $SRC/qtbase
./configure -no-glib -qt-libpng -qt-pcre -opensource -confirm-license -static -no-opengl -no-icu -platform linux-clang-libc++ -debug -prefix /usr -no-feature-widgets -no-feature-sql -no-feature-network  -no-feature-xml -no-feature-dbus -no-feature-printsupport
cmake --build . --parallel $(nproc)
cmake --install .

# Build extra-cmake-modules
cd $SRC/extra-cmake-modules
cmake -G Ninja . -DBUILD_TESTING=OFF -DBUILD_DOC=OFF
ninja install

cd $SRC/karchive
rm -rf poqm
cmake -G Ninja . -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr/local
ninja install

# Build JXRlib
cd $SRC/jxrlib
make -j$(nproc)

# Build LibRaw
cd $SRC/LibRaw
TMP_CFLAGS=$CFLAGS
TMP_CXXFLAGS=$CXXFLAGS
CFLAGS="$CFLAGS -fno-sanitize=function,vptr"
CXXFLAGS="$CXXFLAGS -fno-sanitize=function,vptr"
autoreconf --install
./configure --disable-examples
make -j$(nproc)
make install -j$(nproc)
CFLAGS=$TMP_CFLAGS
CXXFLAGS=$TMP_CXXFLAGS


# Build aom
cd $SRC/aom
mkdir build.libavif
cd build.libavif
extra_libaom_flags='-DAOM_MAX_ALLOCABLE_MEMORY=536870912 -DDO_RANGE_CHECK_CLAMP=1'
cmake -G Ninja -DBUILD_SHARED_LIBS=0 -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 -DCONFIG_PIC=1 -DAOM_TARGET_CPU=generic -DCONFIG_SIZE_LIMIT=1 -DDECODE_HEIGHT_LIMIT=12288 -DDECODE_WIDTH_LIMIT=12288 -DAOM_EXTRA_C_FLAGS="${extra_libaom_flags}" -DAOM_EXTRA_CXX_FLAGS="${extra_libaom_flags}" ..
ninja install

# Build libavif
cd $SRC/libavif
ln -s "$SRC/aom" "$SRC/libavif/ext/"
mkdir build
cd build
CFLAGS="$CFLAGS -fPIC" cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DAVIF_ENABLE_WERROR=OFF -DAVIF_CODEC_AOM=LOCAL -DAVIF_LIBYUV=OFF ..
ninja

# Build libde265
cd $SRC/libde265
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DENABLE_SDL=OFF -DENABLE_SIMD=OFF -DENABLE_AVX2=OFF -DENABLE_AVX512=OFF .
ninja install

# Build openjpeg
cd $SRC/openjpeg
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DBUILD_CODEC=OFF ..
ninja install

# build openh264
cd $SRC/openh264
make USE_ASM=No BUILDTYPE=Debug install-static -j$(nproc)

# Build openexr
cd $SRC/openexr
mkdir _build
cd _build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DOPENEXR_BUILD_TOOLS=OFF -DOPENEXR_INSTALL_TOOLS=OFF -DOPENEXR_BUILD_EXAMPLES=OFF -DOPENEXR_TEST_LIBRARIES=OFF -DOPENEXR_TEST_TOOLS=OFF -DOPENEXR_TEST_PYTHON=OFF ..
ninja install

# Build libjxl
cd $SRC/libjxl
mkdir build
cd build
CXXFLAGS="$CXXFLAGS -DHWY_COMPILE_ONLY_SCALAR" cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_DOXYGEN=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF  -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_PLUGINS=OFF -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_SKCMS=ON -DJPEGXL_ENABLE_TCMALLOC=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_FUZZERS=OFF ..
ninja install

# Build libheif
cd $SRC/libheif
#Reduce max width and height to avoid allocating too much memory
sed -i "s/static const int MAX_IMAGE_WIDTH = 32768;/static const int MAX_IMAGE_WIDTH = 8192;/g" libheif/security_limits.h
sed -i "s/static const int MAX_IMAGE_HEIGHT = 32768;/static const int MAX_IMAGE_HEIGHT = 8192;/g" libheif/security_limits.h
mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DENABLE_PLUGIN_LOADING=OFF -DWITH_EXAMPLES=OFF -DBUILD_DOCUMENTATION=OFF -DWITH_LIBSHARPYUV=OFF -DWITH_UNCOMPRESSED_CODEC=ON -DWITH_DAV1D=OFF -DWITH_EXAMPLES=OFF -DWITH_LIBDE265=ON -DWITH_RAV1E=OFF -DWITH_RAV1E_PLUGIN=OFF -DWITH_SvtEnc=OFF -DWITH_SvtEnc_PLUGIN=OFF -DWITH_X265=OFF -DWITH_X264=OFF -DWITH_OpenJPEG_DECODER=ON -DWITH_OpenH264_DECODER=ON ..
ninja install

cd $SRC/kimageformats
HANDLER_TYPES="ani
        avif
        dds
        exr
        ff
        hdr
        heif
        iff
        jp2
        jxl
        jxr
        kra
        pcx
        pfm
        pic
        psd
        pxr
        qoi
        ras
        raw
        rgb
        sct
        tim
        tga
        xcf"

echo "$HANDLER_TYPES" | while read format; do
(
  fuzz_target_name=kimgio_${format}_fuzzer

  /usr/libexec/moc $SRC/kimageformats/src/imageformats/$format.cpp -o $format.moc
  header=`ls $SRC/kimageformats/src/imageformats/$format*.h`
  /usr/libexec/moc $header -o moc_`basename $header .h`.cpp
  $CXX $CXXFLAGS $LDFLAGS -fPIC -DKIMG_FUZZER_${format}=1 -std=c++17 autotests/ossfuzz/kimgio_fuzzer.cc $SRC/kimageformats/src/imageformats/$format.cpp $SRC/kimageformats/src/imageformats/scanlineconverter.cpp $SRC/kimageformats/src/imageformats/microexif.cpp $SRC/kimageformats/src/imageformats/chunks.cpp -o $OUT/$fuzz_target_name -DJXL_STATIC_DEFINE -DJXL_THREADS_STATIC_DEFINE -DJXL_CMS_STATIC_DEFINE -D__ANSI__ -I $SRC/kimageformats/src/imageformats/ -I $SRC/libavif/include/ -I $SRC/libjxl/build/lib/include/ -I $SRC/libjxl/lib/include/ -I /usr/local/include/OpenEXR/ -I /usr/local/include/KF6/KArchive/ -I /usr/local/include/openjpeg-2.5 -I /usr/local/include/Imath -I $SRC/jxrlib/common/include -I $SRC/jxrlib/jxrgluelib -I $SRC/jxrlib/image/sys -I /usr/include/QtCore/ -I /usr/include/QtGui/ -I . $SRC/libavif/build/libavif.a /usr/local/lib/libheif.a /usr/local/lib/libde265.a /usr/local/lib/libopenh264.a $SRC/aom/build.libavif/libaom.a $SRC/libjxl/build/lib/libjxl_threads.a $SRC/libjxl/build/lib/libjxl.a $SRC/libjxl/build/lib/libjxl_cms.a $SRC/libjxl/build/third_party/highway/libhwy.a $SRC/libjxl/build/third_party/brotli/libbrotlidec.a $SRC/libjxl/build/third_party/brotli/libbrotlienc.a $SRC/libjxl/build/third_party/brotli/libbrotlicommon.a -lQt6Gui -lQt6Core -lQt6BundledLibpng -lQt6BundledHarfbuzz -lm -lQt6BundledPcre2 -ldl -lpthread $LIB_FUZZING_ENGINE /usr/local/lib/libz.a /usr/local/lib/x86_64-linux-gnu/libKF6Archive.a /usr/local/lib/libz.a /usr/local/lib/libraw.a /usr/local/lib/libOpenEXR-3_4.a /usr/local/lib/libIex-3_4.a /usr/local/lib/libImath-3_2.a /usr/local/lib/libIlmThread-3_4.a /usr/local/lib/libOpenEXRCore-3_4.a /usr/local/lib/libOpenEXRUtil-3_4.a /usr/local/lib/libopenjp2.a /usr/local/lib/libzstd.a $SRC/jxrlib/build/libjxrglue.a $SRC/jxrlib/build/libjpegxr.a /usr/local/lib/liblzma.a /usr/local/lib/libbz2.a -lclang_rt.builtins

  # -lclang_rt.builtins in the previous line is a temporary workaround to avoid a linker error "undefined reference to __truncsfhf2". Investigate why this is needed here, but not anywhere else, and possibly remove it.

  find . -name "*.${format}" | zip -q $OUT/${fuzz_target_name}_seed_corpus.zip -@
)
done

# Manually add other supported extensions to HEIF seed corpus
find . -type f \( -name "*.avci" -o -name "*.heic" -o -name "*.hej2" \) | zip -q $OUT/kimgio_heif_fuzzer_seed_corpus.zip -@
