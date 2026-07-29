/*
# SPDX-FileCopyrightText: 2018 Google Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2018 Google Inc.
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
*/

/*
  Usage:
    python infra/helper.py build_image kimageformats
    python infra/helper.py build_fuzzers --sanitizer undefined|address|memory kimageformats
    python infra/helper.py run_fuzzer kimageformats kimgio_[ani|avif|dds|exr|ff|hdr|heif|iff|jp2|jxl|jxr|kra|pcx|pfm|pic|psd|pxr|qoi|ras|raw|rgb|sct|tim|tga|xcf]_fuzzer
*/

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>

#if defined KIMG_FUZZER_ani
#include "ani_p.h"
#define HANDLER ANIHandler
#elif defined KIMG_FUZZER_avif
#include "avif_p.h"
#define HANDLER QAVIFHandler
#elif defined KIMG_FUZZER_dds
#include "dds_p.h"
#define HANDLER QDDSHandler
#elif defined KIMG_FUZZER_exr
#include "exr_p.h"
#define HANDLER EXRHandler
#elif defined KIMG_FUZZER_ff
#include "ff_p.h"
#define HANDLER FFHandler
#elif defined KIMG_FUZZER_hdr
#include "hdr_p.h"
#define HANDLER HDRHandler
#elif defined KIMG_FUZZER_heif
#include "heif_p.h"
#define HANDLER HEIFHandler
#elif defined KIMG_FUZZER_iff
#include "iff_p.h"
#define HANDLER IFFHandler
#elif defined KIMG_FUZZER_jp2
#include "jp2_p.h"
#define HANDLER JP2Handler
#elif defined KIMG_FUZZER_jxl
#include "jxl_p.h"
#define HANDLER QJpegXLHandler
#elif defined KIMG_FUZZER_jxr
#include "jxr_p.h"
#define HANDLER JXRHandler
#elif defined KIMG_FUZZER_kra
#include "kra_p.h"
#define HANDLER KraHandler
#elif defined KIMG_FUZZER_pcx
#include "pcx_p.h"
#define HANDLER PCXHandler
#elif defined KIMG_FUZZER_pfm
#include "pfm_p.h"
#define HANDLER PFMHandler
#elif defined KIMG_FUZZER_pic
#include "pic_p.h"
#define HANDLER SoftimagePICHandler
#elif defined KIMG_FUZZER_psd
#include "psd_p.h"
#define HANDLER PSDHandler
#elif defined KIMG_FUZZER_pxr
#include "pxr_p.h"
#define HANDLER PXRHandler
#elif defined KIMG_FUZZER_qoi
#include "qoi_p.h"
#define HANDLER QOIHandler
#elif defined KIMG_FUZZER_ras
#include "ras_p.h"
#define HANDLER RASHandler
#elif defined KIMG_FUZZER_raw
#include "raw_p.h"
#define HANDLER RAWHandler
#elif defined KIMG_FUZZER_rgb
#include "rgb_p.h"
#define HANDLER RGBHandler
#elif defined KIMG_FUZZER_sct
#include "sct_p.h"
#define HANDLER ScitexHandler
#elif defined KIMG_FUZZER_tim
#include "tim_p.h"
#define HANDLER TIMHandler
#elif defined KIMG_FUZZER_tga
#include "tga_p.h"
#define HANDLER TGAHandler
#elif defined KIMG_FUZZER_xcf
#include "xcf_p.h"
#define HANDLER XCFHandler
#else
#error "KIMG_FUZZER_format not defined!"
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int argc = 0;
    QCoreApplication a(argc, nullptr);

    QImageReader::setAllocationLimit(512);

    QImageIOHandler *handler = new HANDLER();

    QImage i;
    QBuffer b;
    b.setData((const char *)data, size);
    b.open(QIODevice::ReadOnly);
    handler->setDevice(&b);
    handler->canRead();
    handler->read(&i);

    delete handler;

    return 0;
}
