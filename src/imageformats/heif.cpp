/*
    High Efficiency Image File Format (HEIF) support for QImage.

    SPDX-FileCopyrightText: 2020 Sirius Bakke <sirius@bakke.co>
    SPDX-FileCopyrightText: 2021 Daniel Novomesky <dnovomesky@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "heif_p.h"
#include "microexif_p.h"
#include "util_p.h"
#include <libheif/heif.h>

#include <QColorSpace>
#include <QDebug>
#include <QPointF>
#include <QSysInfo>
#include <cstring>
#include <limits>

#ifndef HEIF_MAX_METADATA_SIZE
/*!
 * XMP and EXIF maximum size.
 */
#define HEIF_MAX_METADATA_SIZE (4 * 1024 * 1024)
#endif

size_t HEIFHandler::m_initialized_count = 0;
bool HEIFHandler::m_plugins_queried = false;
bool HEIFHandler::m_heif_decoder_available = false;
bool HEIFHandler::m_heif_encoder_available = false;
bool HEIFHandler::m_hej2_decoder_available = false;
bool HEIFHandler::m_hej2_encoder_available = false;
bool HEIFHandler::m_avci_decoder_available = false;

extern "C" {
static struct heif_error heifhandler_write_callback(struct heif_context * /* ctx */, const void *data, size_t size, void *userdata)
{
    heif_error error;
    error.code = heif_error_Ok;
    error.subcode = heif_suberror_Unspecified;
    error.message = "Success";

    if (!userdata || !data || size == 0) {
        error.code = heif_error_Usage_error;
        error.subcode = heif_suberror_Null_pointer_argument;
        error.message = "Wrong parameters!";
        return error;
    }

    QIODevice *ioDevice = static_cast<QIODevice *>(userdata);
    qint64 bytesWritten = ioDevice->write(static_cast<const char *>(data), size);

    if (bytesWritten < static_cast<qint64>(size)) {
        error.code = heif_error_Encoding_error;
        error.message = "Bytes written to QIODevice are smaller than input data size";
        error.subcode = heif_suberror_Cannot_write_output_data;
    }

    return error;
}
}

HEIFHandler::HEIFHandler()
    : m_parseState(ParseHeicNotParsed)
    , m_quality(100)
{
}

bool HEIFHandler::canRead() const
{
    if (m_parseState == ParseHeicNotParsed) {
        QIODevice *dev = device();
        if (dev) {
            const QByteArray header = dev->peek(28);

            if (HEIFHandler::isSupportedBMFFType(header)) {
                setFormat("heif");
                return true;
            }

            if (HEIFHandler::isSupportedHEJ2(header)) {
                setFormat("hej2");
                return true;
            }

            if (HEIFHandler::isSupportedAVCI(header)) {
                setFormat("avci");
                return true;
            }
        }
        return false;
    }

    if (m_parseState != ParseHeicError) {
        return true;
    }
    return false;
}

bool HEIFHandler::read(QImage *outImage)
{
    if (!ensureParsed()) {
        return false;
    }

    *outImage = m_current_image;
    return true;
}

bool HEIFHandler::write(const QImage &image)
{
    if (image.format() == QImage::Format_Invalid || image.isNull()) {
        qWarning("No image data to save");
        return false;
    }

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    startHeifLib();
#endif

    bool success = write_helper(image);

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    finishHeifLib();
#endif

    return success;
}

bool HEIFHandler::write_helper(const QImage &image)
{
    int save_depth; // 8 or 10bit per channel
    QImage::Format tmpformat; // format for temporary image
    const bool save_alpha = image.hasAlphaChannel();

    switch (image.format()) {
    case QImage::Format_BGR30:
    case QImage::Format_A2BGR30_Premultiplied:
    case QImage::Format_RGB30:
    case QImage::Format_A2RGB30_Premultiplied:
    case QImage::Format_Grayscale16:
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        save_depth = 10;
        break;
    default:
        if (image.depth() > 32) {
            save_depth = 10;
        } else {
            save_depth = 8;
        }
        break;
    }

    heif_compression_format encoder_codec = heif_compression_HEVC;
#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    if (format() == "hej2") {
        encoder_codec = heif_compression_JPEG2000;
        save_depth = 8; // for compatibility reasons
    }
#endif

    heif_chroma chroma;
    if (save_depth > 8) {
        if (save_alpha) {
            tmpformat = QImage::Format_RGBA64;
            chroma = (QSysInfo::ByteOrder == QSysInfo::LittleEndian) ? heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBBAA_BE;
        } else {
            tmpformat = QImage::Format_RGBX64;
            chroma = (QSysInfo::ByteOrder == QSysInfo::LittleEndian) ? heif_chroma_interleaved_RRGGBB_LE : heif_chroma_interleaved_RRGGBB_BE;
        }
    } else {
        if (save_alpha) {
            tmpformat = QImage::Format_RGBA8888;
            chroma = heif_chroma_interleaved_RGBA;
        } else {
            tmpformat = QImage::Format_RGB888;
            chroma = heif_chroma_interleaved_RGB;
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QImage tmpimage;
    auto cs = image.colorSpace();
    if (cs.isValid() && cs.colorModel() == QColorSpace::ColorModel::Cmyk && image.format() == QImage::Format_CMYK8888) {
        tmpimage = image.convertedToColorSpace(QColorSpace(QColorSpace::SRgb), tmpformat);
    } else if (cs.isValid() && cs.colorModel() == QColorSpace::ColorModel::Gray
               && (image.format() == QImage::Format_Grayscale8 || image.format() == QImage::Format_Grayscale16)) {
        QColorSpace::TransferFunction trc_new = cs.transferFunction();
        float gamma_new = cs.gamma();
        if (trc_new == QColorSpace::TransferFunction::Custom) {
            trc_new = QColorSpace::TransferFunction::SRgb;
        }
        tmpimage = image.convertedToColorSpace(QColorSpace(QColorSpace::Primaries::SRgb, trc_new, gamma_new), tmpformat);
    } else {
        tmpimage = image.convertToFormat(tmpformat);
    }
#else
    QImage tmpimage = image.convertToFormat(tmpformat);
#endif

    struct heif_context *context = heif_context_alloc();
    struct heif_error err;
    struct heif_image *h_image = nullptr;

    err = heif_image_create(tmpimage.width(), tmpimage.height(), heif_colorspace_RGB, chroma, &h_image);
    if (err.code) {
        qWarning() << "heif_image_create error:" << err.message;
        heif_context_free(context);
        return false;
    }

    QByteArray iccprofile = tmpimage.colorSpace().iccProfile();
    if (iccprofile.size() > 0) {
        heif_image_set_raw_color_profile(h_image, "prof", iccprofile.constData(), iccprofile.size());
    }

    heif_image_add_plane(h_image, heif_channel_interleaved, image.width(), image.height(), save_depth);
    int stride = 0;
    uint8_t *const dst = heif_image_get_plane(h_image, heif_channel_interleaved, &stride);
    size_t rowbytes;

    switch (save_depth) {
    case 10:
        if (save_alpha) {
            for (int y = 0; y < tmpimage.height(); y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(tmpimage.constScanLine(y));
                uint16_t *dest_word = reinterpret_cast<uint16_t *>(dst + (y * stride));
                for (int x = 0; x < tmpimage.width(); x++) {
                    int tmp_pixelval;
                    // R
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                    // G
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                    // B
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                    // A
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                }
            }
        } else { // no alpha channel
            for (int y = 0; y < tmpimage.height(); y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(tmpimage.constScanLine(y));
                uint16_t *dest_word = reinterpret_cast<uint16_t *>(dst + (y * stride));
                for (int x = 0; x < tmpimage.width(); x++) {
                    int tmp_pixelval;
                    // R
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                    // G
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                    // B
                    tmp_pixelval = (int)(((float)(*src_word) / 65535.0f) * 1023.0f + 0.5f);
                    *dest_word = qBound(0, tmp_pixelval, 1023);
                    src_word++;
                    dest_word++;
                    // X
                    src_word++;
                }
            }
        }
        break;
    case 8:
        rowbytes = save_alpha ? (tmpimage.width() * 4) : (tmpimage.width() * 3);
        for (int y = 0; y < tmpimage.height(); y++) {
            memcpy(dst + (y * stride), tmpimage.constScanLine(y), rowbytes);
        }
        break;
    default:
        qWarning() << "Unsupported depth:" << save_depth;
        heif_image_release(h_image);
        heif_context_free(context);
        return false;
        break;
    }

    struct heif_encoder *encoder = nullptr;
    err = heif_context_get_encoder_for_format(context, encoder_codec, &encoder);
    if (err.code) {
        qWarning() << "Unable to get an encoder instance:" << err.message;
        heif_image_release(h_image);
        heif_context_free(context);
        return false;
    }

    heif_encoder_set_lossy_quality(encoder, m_quality);
    if (m_quality > 90) {
        if (m_quality == 100) {
            heif_encoder_set_lossless(encoder, true);
        }
        heif_encoder_set_parameter_string(encoder, "chroma", "444");
    }

    struct heif_encoding_options *encoder_options = heif_encoding_options_alloc();
    encoder_options->save_alpha_channel = save_alpha;

    if ((tmpimage.width() % 2 == 1) || (tmpimage.height() % 2 == 1)) {
        qWarning() << "Image has odd dimension!\nUse even-numbered dimension(s) for better compatibility with other HEIF implementations.";
        if (save_alpha) {
            // This helps to save alpha channel when image has odd dimension
            encoder_options->macOS_compatibility_workaround = 0;
        }
    }

    struct heif_image_handle *handle;
    err = heif_context_encode_image(context, h_image, encoder, encoder_options, &handle);

    // exif metadata
    if (err.code == heif_error_Ok) {
        auto exif = MicroExif::fromImage(tmpimage);
        if (!exif.isEmpty()) {
            auto ba = exif.toByteArray();
            err = heif_context_add_exif_metadata(context, handle, ba.constData(), ba.size());
        }
    }
    // xmp metadata
    if (err.code == heif_error_Ok) {
        auto xmp = image.text(QStringLiteral(META_KEY_XMP_ADOBE));
        if (!xmp.isEmpty()) {
            auto ba = xmp.toUtf8();
            err = heif_context_add_XMP_metadata(context, handle, ba.constData(), ba.size());
        }
    }

    if (encoder_options) {
        heif_encoding_options_free(encoder_options);
    }

    if (err.code) {
        qWarning() << "heif_context_encode_image failed:" << err.message;
        heif_encoder_release(encoder);
        heif_image_release(h_image);
        heif_context_free(context);
        return false;
    }

    struct heif_writer writer;
    writer.writer_api_version = 1;
    writer.write = heifhandler_write_callback;

    err = heif_context_write(context, &writer, device());

    heif_encoder_release(encoder);
    heif_image_release(h_image);

    if (err.code) {
        qWarning() << "Writing HEIF image failed:" << err.message;
        heif_context_free(context);
        return false;
    }

    heif_context_free(context);
    return true;
}

bool HEIFHandler::isSupportedBMFFType(const QByteArray &header)
{
    if (header.size() < 28) {
        return false;
    }

    const char *buffer = header.constData();
    if (memcmp(buffer + 4, "ftyp", 4) == 0) {
        if (memcmp(buffer + 8, "heic", 4) == 0) {
            return true;
        }
        if (memcmp(buffer + 8, "heis", 4) == 0) {
            return true;
        }
        if (memcmp(buffer + 8, "heix", 4) == 0) {
            return true;
        }

        /* we want to avoid loading AVIF files via this plugin */
        if (memcmp(buffer + 8, "mif1", 4) == 0) {
            for (int offset = 16; offset <= 24; offset += 4) {
                if (memcmp(buffer + offset, "avif", 4) == 0) {
                    return false;
                }
            }
            return true;
        }

        if (memcmp(buffer + 8, "mif2", 4) == 0) {
            return true;
        }
        if (memcmp(buffer + 8, "msf1", 4) == 0) {
            return true;
        }
    }

    return false;
}

bool HEIFHandler::isSupportedHEJ2(const QByteArray &header)
{
    if (header.size() < 28) {
        return false;
    }

    const char *buffer = header.constData();
    if (memcmp(buffer + 4, "ftypj2ki", 8) == 0) {
        return true;
    }

    return false;
}

bool HEIFHandler::isSupportedAVCI(const QByteArray &header)
{
    if (header.size() < 28) {
        return false;
    }

    const char *buffer = header.constData();
    if (memcmp(buffer + 4, "ftypavci", 8) == 0) {
        return true;
    }

    return false;
}

QVariant HEIFHandler::option(ImageOption option) const
{
    if (option == Quality) {
        return m_quality;
    }

    if (!supportsOption(option) || !ensureParsed()) {
        return QVariant();
    }

    switch (option) {
    case Size:
        return m_current_image.size();
        break;
    default:
        return QVariant();
        break;
    }
}

void HEIFHandler::setOption(ImageOption option, const QVariant &value)
{
    switch (option) {
    case Quality:
        m_quality = value.toInt();
        if (m_quality > 100) {
            m_quality = 100;
        } else if (m_quality < 0) {
            m_quality = 100;
        }
        break;
    default:
        QImageIOHandler::setOption(option, value);
        break;
    }
}

bool HEIFHandler::supportsOption(ImageOption option) const
{
    return option == Quality || option == Size;
}

bool HEIFHandler::ensureParsed() const
{
    if (m_parseState == ParseHeicSuccess) {
        return true;
    }
    if (m_parseState == ParseHeicError) {
        return false;
    }

    HEIFHandler *that = const_cast<HEIFHandler *>(this);

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    startHeifLib();
#endif

    bool success = that->ensureDecoder();

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    finishHeifLib();
#endif
    return success;
}

bool HEIFHandler::ensureDecoder()
{
    if (m_parseState != ParseHeicNotParsed) {
        if (m_parseState == ParseHeicSuccess) {
            return true;
        }
        return false;
    }

    const QByteArray buffer = device()->readAll();
    if (!HEIFHandler::isSupportedBMFFType(buffer) && !HEIFHandler::isSupportedHEJ2(buffer) && !HEIFHandler::isSupportedAVCI(buffer)) {
        m_parseState = ParseHeicError;
        return false;
    }

    struct heif_context *ctx = heif_context_alloc();
    struct heif_error err = heif_context_read_from_memory(ctx, static_cast<const void *>(buffer.constData()), buffer.size(), nullptr);

    if (err.code) {
        qWarning() << "heif_context_read_from_memory error:" << err.message;
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        return false;
    }

    struct heif_image_handle *handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code) {
        qWarning() << "heif_context_get_primary_image_handle error:" << err.message;
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        return false;
    }

    if ((heif_image_handle_get_width(handle) == 0) || (heif_image_handle_get_height(handle) == 0)) {
        m_parseState = ParseHeicError;
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        qWarning() << "HEIC image has zero dimension";
        return false;
    }

    const int bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);

    if (bit_depth < 8) {
        m_parseState = ParseHeicError;
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        qWarning() << "HEIF image with undefined or unsupported bit depth.";
        return false;
    }

    const bool hasAlphaChannel = heif_image_handle_has_alpha_channel(handle);
    heif_chroma chroma;

    QImage::Format target_image_format;

    if (bit_depth == 10 || bit_depth == 12 || bit_depth == 16) {
        if (hasAlphaChannel) {
            chroma = (QSysInfo::ByteOrder == QSysInfo::LittleEndian) ? heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBBAA_BE;
            target_image_format = QImage::Format_RGBA64;
        } else {
            chroma = (QSysInfo::ByteOrder == QSysInfo::LittleEndian) ? heif_chroma_interleaved_RRGGBB_LE : heif_chroma_interleaved_RRGGBB_BE;
            target_image_format = QImage::Format_RGBX64;
        }
    } else if (bit_depth == 8) {
        if (hasAlphaChannel) {
            chroma = heif_chroma_interleaved_RGBA;
            target_image_format = QImage::Format_ARGB32;
        } else {
            chroma = heif_chroma_interleaved_RGB;
            target_image_format = QImage::Format_RGB32;
        }
    } else {
        m_parseState = ParseHeicError;
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        qWarning() << "Unsupported bit depth:" << bit_depth;
        return false;
    }

    struct heif_decoding_options *decoder_option = heif_decoding_options_alloc();

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    decoder_option->strict_decoding = 1;
#endif

    struct heif_image *img = nullptr;
    err = heif_decode_image(handle, &img, heif_colorspace_RGB, chroma, decoder_option);

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    if (err.code == heif_error_Invalid_input && err.subcode == heif_suberror_Unknown_NCLX_matrix_coefficients && img == nullptr && buffer.contains("Xiaomi")) {
        qWarning() << "Non-standard HEIF image with invalid matrix_coefficients, probably made by a Xiaomi device!";

        // second try to decode with strict decoding disabled
        decoder_option->strict_decoding = 0;
        err = heif_decode_image(handle, &img, heif_colorspace_RGB, chroma, decoder_option);
    }
#endif

    if (decoder_option) {
        heif_decoding_options_free(decoder_option);
    }

    if (err.code) {
        qWarning() << "heif_decode_image error:" << err.message;
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        return false;
    }

    const int imageWidth = heif_image_get_width(img, heif_channel_interleaved);
    const int imageHeight = heif_image_get_height(img, heif_channel_interleaved);

    QSize imageSize(imageWidth, imageHeight);

    if (!imageSize.isValid()) {
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        qWarning() << "HEIC image size invalid:" << imageSize;
        return false;
    }

    int stride = 0;
    const uint8_t *const src = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

    if (!src || stride <= 0) {
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        qWarning() << "HEIC data pixels information not valid!";
        return false;
    }

    m_current_image = imageAlloc(imageSize, target_image_format);
    if (m_current_image.isNull()) {
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        qWarning() << "Unable to allocate memory!";
        return false;
    }

    switch (bit_depth) {
    case 16:
        if (hasAlphaChannel) {
            for (int y = 0; y < imageHeight; y++) {
                memcpy(m_current_image.scanLine(y), src + (y * stride), 8 * size_t(imageWidth));
            }
        } else { // no alpha channel
            for (int y = 0; y < imageHeight; y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(src + (y * stride));
                uint16_t *dest_data = reinterpret_cast<uint16_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    // R
                    *dest_data = *src_word;
                    src_word++;
                    dest_data++;
                    // G
                    *dest_data = *src_word;
                    src_word++;
                    dest_data++;
                    // B
                    *dest_data = *src_word;
                    src_word++;
                    dest_data++;
                    // X = 0xffff
                    *dest_data = 0xffff;
                    dest_data++;
                }
            }
        }
        break;
    case 12:
        if (hasAlphaChannel) {
            for (int y = 0; y < imageHeight; y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(src + (y * stride));
                uint16_t *dest_data = reinterpret_cast<uint16_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    int tmpvalue;
                    // R
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // G
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // B
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // A
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                }
            }
        } else { // no alpha channel
            for (int y = 0; y < imageHeight; y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(src + (y * stride));
                uint16_t *dest_data = reinterpret_cast<uint16_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    int tmpvalue;
                    // R
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // G
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // B
                    tmpvalue = (int)(((float)(0x0fff & (*src_word)) / 4095.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // X = 0xffff
                    *dest_data = 0xffff;
                    dest_data++;
                }
            }
        }
        break;
    case 10:
        if (hasAlphaChannel) {
            for (int y = 0; y < imageHeight; y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(src + (y * stride));
                uint16_t *dest_data = reinterpret_cast<uint16_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    int tmpvalue;
                    // R
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // G
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // B
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // A
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                }
            }
        } else { // no alpha channel
            for (int y = 0; y < imageHeight; y++) {
                const uint16_t *src_word = reinterpret_cast<const uint16_t *>(src + (y * stride));
                uint16_t *dest_data = reinterpret_cast<uint16_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    int tmpvalue;
                    // R
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // G
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // B
                    tmpvalue = (int)(((float)(0x03ff & (*src_word)) / 1023.0f) * 65535.0f + 0.5f);
                    tmpvalue = qBound(0, tmpvalue, 65535);
                    *dest_data = (uint16_t)tmpvalue;
                    src_word++;
                    dest_data++;
                    // X = 0xffff
                    *dest_data = 0xffff;
                    dest_data++;
                }
            }
        }
        break;
    case 8:
        if (hasAlphaChannel) {
            for (int y = 0; y < imageHeight; y++) {
                const uint8_t *src_byte = src + (y * stride);
                uint32_t *dest_pixel = reinterpret_cast<uint32_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    int red = *src_byte++;
                    int green = *src_byte++;
                    int blue = *src_byte++;
                    int alpha = *src_byte++;
                    *dest_pixel = qRgba(red, green, blue, alpha);
                    dest_pixel++;
                }
            }
        } else { // no alpha channel
            for (int y = 0; y < imageHeight; y++) {
                const uint8_t *src_byte = src + (y * stride);
                uint32_t *dest_pixel = reinterpret_cast<uint32_t *>(m_current_image.scanLine(y));
                for (int x = 0; x < imageWidth; x++) {
                    int red = *src_byte++;
                    int green = *src_byte++;
                    int blue = *src_byte++;
                    *dest_pixel = qRgb(red, green, blue);
                    dest_pixel++;
                }
            }
        }
        break;
    default:
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        m_parseState = ParseHeicError;
        qWarning() << "Unsupported bit depth:" << bit_depth;
        return false;
        break;
    }

    heif_color_profile_type profileType = heif_image_handle_get_color_profile_type(handle);
    if (profileType == heif_color_profile_type_prof || profileType == heif_color_profile_type_rICC) {
        size_t rawProfileSize = heif_image_handle_get_raw_color_profile_size(handle);
        if (rawProfileSize > 0 && rawProfileSize < std::numeric_limits<int>::max()) {
            QByteArray ba(rawProfileSize, 0);
            err = heif_image_handle_get_raw_color_profile(handle, ba.data());
            if (err.code) {
                qWarning() << "icc profile loading failed";
            } else {
                QColorSpace colorspace = QColorSpace::fromIccProfile(ba);
                if (!colorspace.isValid()) {
                    qWarning() << "HEIC image has Qt-unsupported or invalid ICC profile!";
                }
#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
                else if (colorspace.colorModel() == QColorSpace::ColorModel::Cmyk) {
                    qWarning("CMYK ICC profile is not expected for HEIF, discarding the ICCprofile!");
                    colorspace = QColorSpace();
                } else if (colorspace.colorModel() == QColorSpace::ColorModel::Gray) {
                    if (hasAlphaChannel) {
                        QPointF gray_whitePoint = colorspace.whitePoint();
                        if (gray_whitePoint.isNull()) {
                            gray_whitePoint = QPointF(0.3127f, 0.329f);
                        }

                        const QPointF redP(0.64f, 0.33f);
                        const QPointF greenP(0.3f, 0.6f);
                        const QPointF blueP(0.15f, 0.06f);

                        QColorSpace::TransferFunction trc_new = colorspace.transferFunction();
                        float gamma_new = colorspace.gamma();
                        if (trc_new == QColorSpace::TransferFunction::Custom) {
                            trc_new = QColorSpace::TransferFunction::SRgb;
                        }
                        colorspace = QColorSpace(gray_whitePoint, redP, greenP, blueP, trc_new, gamma_new);
                        if (!colorspace.isValid()) {
                            qWarning("HEIF plugin created invalid QColorSpace!");
                        }
                    } else { // no alpha channel
                        m_current_image.convertTo(bit_depth > 8 ? QImage::Format_Grayscale16 : QImage::Format_Grayscale8);
                    }
                }
#endif
                m_current_image.setColorSpace(colorspace);
            }
        } else {
            qWarning() << "icc profile is empty or above limits";
        }

    } else if (profileType == heif_color_profile_type_nclx) {
        struct heif_color_profile_nclx *nclx = nullptr;
        err = heif_image_handle_get_nclx_color_profile(handle, &nclx);
        if (err.code || !nclx) {
            qWarning() << "nclx profile loading failed";
        } else {
            const QPointF redPoint(nclx->color_primary_red_x, nclx->color_primary_red_y);
            const QPointF greenPoint(nclx->color_primary_green_x, nclx->color_primary_green_y);
            const QPointF bluePoint(nclx->color_primary_blue_x, nclx->color_primary_blue_y);
            const QPointF whitePoint(nclx->color_primary_white_x, nclx->color_primary_white_y);

            QColorSpace::TransferFunction q_trc = QColorSpace::TransferFunction::Custom;
            float q_trc_gamma = 0.0f;

            switch (nclx->transfer_characteristics) {
            case 4:
                q_trc = QColorSpace::TransferFunction::Gamma;
                q_trc_gamma = 2.2f;
                break;
            case 5:
                q_trc = QColorSpace::TransferFunction::Gamma;
                q_trc_gamma = 2.8f;
                break;
            case 8:
                q_trc = QColorSpace::TransferFunction::Linear;
                break;
            case 2:
            case 13:
                q_trc = QColorSpace::TransferFunction::SRgb;
                break;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
            case 16:
                q_trc = QColorSpace::TransferFunction::St2084;
                break;
            case 18:
                q_trc = QColorSpace::TransferFunction::Hlg;
                break;
#endif
            default:
                qWarning("CICP color_primaries: %d, transfer_characteristics: %d\nThe colorspace is unsupported by this plug-in yet.",
                         nclx->color_primaries,
                         nclx->transfer_characteristics);
                q_trc = QColorSpace::TransferFunction::SRgb;
                break;
            }

            if (q_trc != QColorSpace::TransferFunction::Custom) { // we create new colorspace using Qt
                switch (nclx->color_primaries) {
                case 1:
                case 2:
                    m_current_image.setColorSpace(QColorSpace(QColorSpace::Primaries::SRgb, q_trc, q_trc_gamma));
                    break;
                case 12:
                    m_current_image.setColorSpace(QColorSpace(QColorSpace::Primaries::DciP3D65, q_trc, q_trc_gamma));
                    break;
                default:
                    m_current_image.setColorSpace(QColorSpace(whitePoint, redPoint, greenPoint, bluePoint, q_trc, q_trc_gamma));
                    break;
                }
            }
            heif_nclx_color_profile_free(nclx);

            if (!m_current_image.colorSpace().isValid()) {
                qWarning() << "HEIC plugin created invalid QColorSpace from NCLX!";
            }
        }

    } else {
        m_current_image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    }

    // read metadata
    if (auto numMetadata = heif_image_handle_get_number_of_metadata_blocks(handle, nullptr)) {
        QVector<heif_item_id> ids(numMetadata);
        heif_image_handle_get_list_of_metadata_block_IDs(handle, nullptr, ids.data(), numMetadata);
        for (int n = 0; n < numMetadata; ++n) {
            auto itemtype = heif_image_handle_get_metadata_type(handle, ids[n]);
            auto contenttype = heif_image_handle_get_metadata_content_type(handle, ids[n]);
            auto isExif = !std::strcmp(itemtype, "Exif");
            auto isXmp = !std::strcmp(contenttype, "application/rdf+xml");
            if (isExif || isXmp) {
                auto sz = heif_image_handle_get_metadata_size(handle, ids[n]);
                if (sz == 0 || sz >= HEIF_MAX_METADATA_SIZE)
                    continue;
                QByteArray ba(sz, char());
                auto err = heif_image_handle_get_metadata(handle, ids[n], ba.data());
                if (err.code != heif_error_Ok) {
                    qWarning() << "Error while reading metadata" << err.message;
                    continue;
                }
                if (isXmp) {
                    m_current_image.setText(QStringLiteral(META_KEY_XMP_ADOBE), QString::fromUtf8(ba));
                } else if (isExif) {
                    auto exif = MicroExif::fromByteArray(ba, true);
                    if (!exif.isEmpty()) {
                        exif.updateImageResolution(m_current_image);
                        exif.updateImageMetadata(m_current_image);
                    }
                }
            }
        }
    }

    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    m_parseState = ParseHeicSuccess;
    return true;
}

bool HEIFHandler::isHeifDecoderAvailable()
{
    HEIFHandler::queryHeifLib();

    return m_heif_decoder_available;
}

bool HEIFHandler::isHeifEncoderAvailable()
{
    HEIFHandler::queryHeifLib();

    return m_heif_encoder_available;
}

bool HEIFHandler::isHej2DecoderAvailable()
{
    HEIFHandler::queryHeifLib();

    return m_hej2_decoder_available;
}

bool HEIFHandler::isHej2EncoderAvailable()
{
    HEIFHandler::queryHeifLib();

    return m_hej2_encoder_available;
}

bool HEIFHandler::isAVCIDecoderAvailable()
{
    HEIFHandler::queryHeifLib();

    return m_avci_decoder_available;
}

void HEIFHandler::queryHeifLib()
{
    QMutexLocker locker(&getHEIFHandlerMutex());

    if (!m_plugins_queried) {
#if LIBHEIF_HAVE_VERSION(1, 13, 0)
        if (m_initialized_count == 0) {
            heif_init(nullptr);
        }
#endif

        m_heif_encoder_available = heif_have_encoder_for_format(heif_compression_HEVC);
        m_heif_decoder_available = heif_have_decoder_for_format(heif_compression_HEVC);
#if LIBHEIF_HAVE_VERSION(1, 13, 0)
        m_hej2_decoder_available = heif_have_decoder_for_format(heif_compression_JPEG2000);
        m_hej2_encoder_available = heif_have_encoder_for_format(heif_compression_JPEG2000);
#endif
#if LIBHEIF_HAVE_VERSION(1, 19, 6)
        m_avci_decoder_available = heif_have_decoder_for_format(heif_compression_AVC);
#endif
        m_plugins_queried = true;

#if LIBHEIF_HAVE_VERSION(1, 13, 0)
        if (m_initialized_count == 0) {
            heif_deinit();
        }
#endif
    }
}

void HEIFHandler::startHeifLib()
{
#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    QMutexLocker locker(&getHEIFHandlerMutex());

    if (m_initialized_count == 0) {
        heif_init(nullptr);
    }

    m_initialized_count++;
#endif
}

void HEIFHandler::finishHeifLib()
{
#if LIBHEIF_HAVE_VERSION(1, 13, 0)
    QMutexLocker locker(&getHEIFHandlerMutex());

    if (m_initialized_count == 0) {
        return;
    }

    m_initialized_count--;
    if (m_initialized_count == 0) {
        heif_deinit();
    }

#endif
}

QMutex &HEIFHandler::getHEIFHandlerMutex()
{
    static QMutex heif_handler_mutex;
    return heif_handler_mutex;
}

QImageIOPlugin::Capabilities HEIFPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "heif" || format == "heic") {
        Capabilities format_cap;
        if (HEIFHandler::isHeifDecoderAvailable()) {
            format_cap |= CanRead;
        }
        if (HEIFHandler::isHeifEncoderAvailable()) {
            format_cap |= CanWrite;
        }
        return format_cap;
    }

    if (format == "hej2") {
        Capabilities format_cap;
        if (HEIFHandler::isHej2DecoderAvailable()) {
            format_cap |= CanRead;
        }
        if (HEIFHandler::isHej2EncoderAvailable()) {
            format_cap |= CanWrite;
        }
        return format_cap;
    }

    if (format == "avci") {
        Capabilities format_cap;
        if (HEIFHandler::isAVCIDecoderAvailable()) {
            format_cap |= CanRead;
        }
        return format_cap;
    }

    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable()) {
        const QByteArray header = device->peek(28);

        if ((HEIFHandler::isSupportedBMFFType(header) && HEIFHandler::isHeifDecoderAvailable())
            || (HEIFHandler::isSupportedHEJ2(header) && HEIFHandler::isHej2DecoderAvailable())
            || (HEIFHandler::isSupportedAVCI(header) && HEIFHandler::isAVCIDecoderAvailable())) {
            cap |= CanRead;
        }
    }

    if (device->isWritable() && (HEIFHandler::isHeifEncoderAvailable() || HEIFHandler::isHej2EncoderAvailable())) {
        cap |= CanWrite;
    }
    return cap;
}

QImageIOHandler *HEIFPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new HEIFHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_heif_p.cpp"
