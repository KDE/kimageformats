/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Dominik Seichter <domseichter@web.de>
    SPDX-FileCopyrightText: 2004 Ignacio Castaño <castano@ludicon.com>
    SPDX-FileCopyrightText: 2025 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/* this code supports:
 * reading:
 *     uncompressed and run length encoded indexed, grey and color tga files.
 *     image types 1, 2, 3, 9, 10 and 11.
 *     only RGB color maps with no more than 256 colors.
 *     pixel formats 8, 16, 24 and 32.
 * writing:
 *     uncompressed true color tga files
 *     uncompressed grayscale tga files
 *     uncompressed indexed tga files
 */

#include "tga_p.h"
#include "util_p.h"

#include <assert.h>

#include <QColorSpace>
#include <QDataStream>
#include <QDebug>
#include <QImage>

typedef quint32 uint;
typedef quint16 ushort;
typedef quint8 uchar;

namespace // Private.
{
// Header format of saved files.
uchar targaMagic[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};

enum TGAType {
    TGA_TYPE_INDEXED = 1,
    TGA_TYPE_RGB = 2,
    TGA_TYPE_GREY = 3,
    TGA_TYPE_RLE_INDEXED = 9,
    TGA_TYPE_RLE_RGB = 10,
    TGA_TYPE_RLE_GREY = 11,
};

#define TGA_INTERLEAVE_MASK 0xc0
#define TGA_INTERLEAVE_NONE 0x00
#define TGA_INTERLEAVE_2WAY 0x40
#define TGA_INTERLEAVE_4WAY 0x80

#define TGA_ORIGIN_MASK 0x30
#define TGA_ORIGIN_LEFT 0x00
#define TGA_ORIGIN_RIGHT 0x10
#define TGA_ORIGIN_LOWER 0x00
#define TGA_ORIGIN_UPPER 0x20

/** Tga Header. */
struct TgaHeader {
    uchar id_length = 0;
    uchar colormap_type = 0;
    uchar image_type = 0;
    ushort colormap_index = 0;
    ushort colormap_length = 0;
    uchar colormap_size = 0;
    ushort x_origin = 0;
    ushort y_origin = 0;
    ushort width = 0;
    ushort height = 0;
    uchar pixel_size = 0;
    uchar flags = 0;

    enum {
        SIZE = 18,
    }; // const static int SIZE = 18;
};

static QDataStream &operator>>(QDataStream &s, TgaHeader &head)
{
    s >> head.id_length;
    s >> head.colormap_type;
    s >> head.image_type;
    s >> head.colormap_index;
    s >> head.colormap_length;
    s >> head.colormap_size;
    s >> head.x_origin;
    s >> head.y_origin;
    s >> head.width;
    s >> head.height;
    s >> head.pixel_size;
    s >> head.flags;
    return s;
}

static bool IsSupported(const TgaHeader &head)
{
    if (head.image_type != TGA_TYPE_INDEXED && head.image_type != TGA_TYPE_RGB && head.image_type != TGA_TYPE_GREY && head.image_type != TGA_TYPE_RLE_INDEXED
        && head.image_type != TGA_TYPE_RLE_RGB && head.image_type != TGA_TYPE_RLE_GREY) {
        return false;
    }
    if (head.image_type == TGA_TYPE_INDEXED || head.image_type == TGA_TYPE_RLE_INDEXED) {
        // GIMP saves TGAs with palette size of 257 (but 256 used) so, I need to check the pixel size only.
        if (head.pixel_size > 8 || head.colormap_type != 1) {
            return false;
        }
        // colormap_size == 16 would be ARRRRRGG GGGBBBBB but we don't support that.
        if (head.colormap_size != 24 && head.colormap_size != 32) {
            return false;
        }
    }
    if (head.image_type == TGA_TYPE_RGB || head.image_type == TGA_TYPE_GREY || head.image_type == TGA_TYPE_RLE_RGB || head.image_type == TGA_TYPE_RLE_GREY) {
        if (head.colormap_type != 0) {
            return false;
        }
    }
    if (head.width == 0 || head.height == 0) {
        return false;
    }
    if (head.pixel_size != 8 && head.pixel_size != 16 && head.pixel_size != 24 && head.pixel_size != 32) {
        return false;
    }
    // If the colormap_type field is set to zero, indicating that no color map exists, then colormap_index and colormap_length should be set to zero.
    if (head.colormap_type == 0 && (head.colormap_index != 0 || head.colormap_length != 0)) {
        return false;
    }

    return true;
}

struct Color555 {
    ushort b : 5;
    ushort g : 5;
    ushort r : 5;
};

struct TgaHeaderInfo {
    bool rle;
    bool pal;
    bool rgb;
    bool grey;

    TgaHeaderInfo(const TgaHeader &tga)
        : rle(false)
        , pal(false)
        , rgb(false)
        , grey(false)
    {
        switch (tga.image_type) {
        case TGA_TYPE_RLE_INDEXED:
            rle = true;
            Q_FALLTHROUGH();
        // no break is intended!
        case TGA_TYPE_INDEXED:
            pal = true;
            break;

        case TGA_TYPE_RLE_RGB:
            rle = true;
            Q_FALLTHROUGH();
        // no break is intended!
        case TGA_TYPE_RGB:
            rgb = true;
            break;

        case TGA_TYPE_RLE_GREY:
            rle = true;
            Q_FALLTHROUGH();
        // no break is intended!
        case TGA_TYPE_GREY:
            grey = true;
            break;

        default:
            // Error, unknown image type.
            break;
        }
    }
};

static QImage::Format imageFormat(const TgaHeader &head)
{
    auto format = QImage::Format_Invalid;
    if (IsSupported(head)) {
        TgaHeaderInfo info(head);

        // Bits 0-3 are the numbers of alpha bits (can be zero!)
        const int numAlphaBits = head.flags & 0xf;
        // However alpha should exists only in the 32 bit format.
        if ((head.pixel_size == 32) && (numAlphaBits)) {
            if (numAlphaBits <= 8) {
                format = QImage::Format_ARGB32;
            }
        // Anyway, GIMP also saves gray images with alpha in TGA format
        } else if ((info.grey) && (head.pixel_size == 16) && (numAlphaBits)) {
            if (numAlphaBits == 8) {
                format = QImage::Format_ARGB32;
            }
        } else if (info.grey) {
            format = QImage::Format_Grayscale8;
        } else if (info.pal) {
            format = QImage::Format_Indexed8;
        } else {
            format = QImage::Format_RGB32;
        }
    }
    return format;
}

/*!
 * \brief peekHeader
 * Reads the header but does not change the position in the device.
 */
static bool peekHeader(QIODevice *device, TgaHeader &header)
{
    auto head = device->peek(TgaHeader::SIZE);
    if (head.size() < TgaHeader::SIZE) {
        return false;
    }
    QDataStream stream(head);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream >> header;
    return true;
}

/*!
 * \brief readTgaLine
 * Read a scan line from the raw data.
 * \param dev The current device.
 * \param pixel_size The number of bytes per pixel.
 * \param size The size of the uncompressed TGA raw line
 * \param rle True if the stream is RLE compressed, otherwise false.
 * \param cache The cache buffer used to store data (only used when the stream is RLE).
 * \return The uncompressed raw data of a line or an empty array on error.
 */
static QByteArray readTgaLine(QIODevice *dev, qint32 pixel_size, qint32 size, bool rle, QByteArray &cache)
{
    // uncompressed stream
    if (!rle) {
        auto ba = dev->read(size);
        if (ba.size() != size)
            ba.clear();
        return ba;
    }

    // RLE compressed stream
    if (cache.size() < qsizetype(size)) {
        // Decode image.
        qint64 num = size;

        while (num > 0) {
            if (dev->atEnd()) {
                break;
            }

            // Get packet header.
            char cc;
            if (dev->read(&cc, 1) != 1) {
                cache.clear();
                break;
            }
            auto c = uchar(cc);

            uint count = (c & 0x7f) + 1;
            QByteArray tmp(count * pixel_size, char());
            auto dst = tmp.data();
            num -= count * pixel_size;

            if (c & 0x80) { // RLE pixels.
                assert(pixel_size <= 8);
                char pixel[8];
                const int dataRead = dev->read(pixel, pixel_size);
                if (dataRead < (int)pixel_size) {
                    memset(&pixel[dataRead], 0, pixel_size - dataRead);
                }
                do {
                    memcpy(dst, pixel, pixel_size);
                    dst += pixel_size;
                } while (--count);
            } else { // Raw pixels.
                count *= pixel_size;
                const int dataRead = dev->read(dst, count);
                if (dataRead < 0) {
                    cache.clear();
                    break;
                }

                if ((uint)dataRead < count) {
                    const size_t toCopy = count - dataRead;
                    memset(&dst[dataRead], 0, toCopy);
                }
                dst += count;
            }

            cache.append(tmp);
        }
    }

    auto data = cache.left(size);
    cache.remove(0, size);
    if (data.size() != size)
        data.clear();
    return data;
}

static bool LoadTGA(QIODevice *dev, const TgaHeader &tga, QImage &img)
{
    img = imageAlloc(tga.width, tga.height, imageFormat(tga));
    if (img.isNull()) {
        qWarning() << "LoadTGA: Failed to allocate image, invalid dimensions?" << QSize(tga.width, tga.height);
        return false;
    }

    TgaHeaderInfo info(tga);

    const int numAlphaBits = tga.flags & 0xf;
    bool hasAlpha = img.hasAlphaChannel();
    qint32 pixel_size = (tga.pixel_size / 8);
    qint32 line_size = qint32(tga.width) * pixel_size;
    qint64 size = qint64(tga.height) * line_size;
    if (size < 1) {
        //          qDebug() << "This TGA file is broken with size " << size;
        return false;
    }

    // Read palette.
    if (info.pal) {
        QList<QRgb> colorTable;
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        colorTable.resize(tga.colormap_length);
#else
        colorTable.resizeForOverwrite(tga.colormap_length);
#endif

        if (tga.colormap_size == 32) { // BGRA.
            char data[4];
            for (QRgb &rgb : colorTable) {
                const auto dataRead = dev->read(data, 4);
                if (dataRead < 4) {
                    return false;
                }
                // BGRA.
                rgb = qRgba(data[2], data[1], data[0], data[3]);
            }
        } else if (tga.colormap_size == 24) { // BGR.
            char data[3];
            for (QRgb &rgb : colorTable) {
                const auto dataRead = dev->read(data, 3);
                if (dataRead < 3) {
                    return false;
                }
                // BGR.
                rgb = qRgb(data[2], data[1], data[0]);
            }
            // TODO tga.colormap_size == 16 ARRRRRGG GGGBBBBB
        } else {
            return false;
        }

        img.setColorTable(colorTable);
    }

    // Convert image to internal format.
    bool valid = true;
    int y_start;
    int y_step;
    int y_end;
    if (tga.flags & TGA_ORIGIN_UPPER) {
        y_start = 0;
        y_step = 1;
        y_end = tga.height;
    } else {
        y_start = tga.height - 1;
        y_step = -1;
        y_end = -1;
    }

    QByteArray cache;
    for (int y = y_start; y != y_end; y += y_step) {
        auto tgaLine = readTgaLine(dev, pixel_size, line_size, info.rle, cache);
        if (tgaLine.size() != qsizetype(line_size)) {
            qWarning() << "LoadTGA: Error while decoding a TGA raw line";
            valid = false;
            break;
        }
        auto src = tgaLine.data();
        if (info.pal) {
            // Paletted.
            auto scanline = img.scanLine(y);
            for (int x = 0; x < tga.width; x++) {
                uchar idx = *src++;
                if (Q_UNLIKELY(idx >= tga.colormap_length)) {
                    valid = false;
                    break;
                }
                scanline[x] = idx;
            }
        } else if (info.grey) {
            if (tga.pixel_size == 16) { // Greyscale with alpha.
                auto scanline = reinterpret_cast<QRgb *>(img.scanLine(y));
                for (int x = 0; x < tga.width; x++) {
                    scanline[x] = qRgba(*src, *src, *src, *(src + 1));
                    src += 2;
                }
            } else { // Greyscale.
                auto scanline = img.scanLine(y);
                for (int x = 0; x < tga.width; x++) {
                    scanline[x] = *src;
                    src++;
                }
            }
        } else {
            auto scanline = reinterpret_cast<QRgb *>(img.scanLine(y));
            // True Color.
            if (tga.pixel_size == 16) {
                for (int x = 0; x < tga.width; x++) {
                    Color555 c = *reinterpret_cast<Color555 *>(src);
                    scanline[x] = qRgb((c.r << 3) | (c.r >> 2), (c.g << 3) | (c.g >> 2), (c.b << 3) | (c.b >> 2));
                    src += 2;
                }
            } else if (tga.pixel_size == 24) {
                for (int x = 0; x < tga.width; x++) {
                    scanline[x] = qRgb(src[2], src[1], src[0]);
                    src += 3;
                }
            } else if (tga.pixel_size == 32) {
                auto div = (1 << numAlphaBits) - 1;
                if (div == 0)
                    hasAlpha = false;
                for (int x = 0; x < tga.width; x++) {
                    // ### TODO: verify with images having really some alpha data
                    const int alpha = hasAlpha ? int((src[3]) << (8 - numAlphaBits)) * 255 / div : 255;
                    scanline[x] = qRgba(src[2], src[1], src[0], alpha);
                    src += 4;
                }
            }
        }
    }

#ifdef QT_DEBUG
    if (!cache.isEmpty() && valid) {
        qDebug() << "LoadTGA: Found unused image data";
    }
#endif

    return valid;
}

} // namespace

class TGAHandlerPrivate
{
public:
    TGAHandlerPrivate() {}
    ~TGAHandlerPrivate() {}

    TgaHeader m_header;
};

TGAHandler::TGAHandler()
    : QImageIOHandler()
    , d(new TGAHandlerPrivate)
{
}

bool TGAHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("tga");
        return true;
    }
    return false;
}

bool TGAHandler::read(QImage *outImage)
{
    // qDebug() << "Loading TGA file!";

    auto dev = device();
    auto&& tga = d->m_header;
    if (!peekHeader(dev, tga) || !IsSupported(tga)) {
        //         qDebug() << "This TGA file is not valid.";
        return false;
    }

    if (dev->isSequential()) {
        dev->read(TgaHeader::SIZE + tga.id_length);
    } else {
        dev->seek(TgaHeader::SIZE + tga.id_length);
    }

    // Check image file format.
    if (dev->atEnd()) {
        //         qDebug() << "This TGA file is not valid.";
        return false;
    }

    QImage img;
    if (!LoadTGA(dev, tga, img)) {
        //         qDebug() << "Error loading TGA file.";
        return false;
    }

    *outImage = img;
    return true;
}

bool TGAHandler::write(const QImage &image)
{
    if (image.format() == QImage::Format_Indexed8)
        return writeIndexed(image);
    if (image.format() == QImage::Format_Grayscale8 || image.format() == QImage::Format_Grayscale16)
        return writeGrayscale(image);
    return writeRGBA(image);
}

bool TGAHandler::writeIndexed(const QImage &image)
{
    QDataStream s(device());
    s.setByteOrder(QDataStream::LittleEndian);

    QImage img(image);
    auto ct = img.colorTable();

    s << quint8(0); // ID Length
    s << quint8(1); // Color Map Type
    s << quint8(TGA_TYPE_INDEXED); // Image Type
    s << quint16(0); // First Entry Index
    s << quint16(ct.size()); // Color Map Length
    s << quint8(32); // Color map Entry Size
    s << quint16(0); // X-origin of Image
    s << quint16(0); // Y-origin of Image

    s << quint16(img.width()); // Image Width
    s << quint16(img.height()); // Image Height
    s << quint8(8); // Pixel Depth
    s << quint8(TGA_ORIGIN_UPPER + TGA_ORIGIN_LEFT); // Image Descriptor

    for (auto &&rgb : ct) {
        s << quint8(qBlue(rgb));
        s << quint8(qGreen(rgb));
        s << quint8(qRed(rgb));
        s << quint8(qAlpha(rgb));
    }

    if (s.status() != QDataStream::Ok) {
        return false;
    }

    for (int y = 0; y < img.height(); y++) {
        auto ptr = img.constScanLine(y);
        for (int x = 0; x < img.width(); x++) {
            s << *(ptr + x);
        }
        if (s.status() != QDataStream::Ok) {
            return false;
        }
    }

    return true;
}

bool TGAHandler::writeGrayscale(const QImage &image)
{
    QDataStream s(device());
    s.setByteOrder(QDataStream::LittleEndian);

    QImage img(image);
    if (img.format() != QImage::Format_Grayscale8) {
        img = img.convertToFormat(QImage::Format_Grayscale8);
    }
    if (img.isNull()) {
        qCritical() << "TGAHandler::writeGrayscale: image conversion to 8 bits grayscale failed!";
        return false;
    }

    s << quint8(0); // ID Length
    s << quint8(0); // Color Map Type
    s << quint8(TGA_TYPE_GREY); // Image Type
    s << quint16(0); // First Entry Index
    s << quint16(0); // Color Map Length
    s << quint8(0); // Color map Entry Size
    s << quint16(0); // X-origin of Image
    s << quint16(0); // Y-origin of Image

    s << quint16(img.width()); // Image Width
    s << quint16(img.height()); // Image Height
    s << quint8(8); // Pixel Depth
    s << quint8(TGA_ORIGIN_UPPER + TGA_ORIGIN_LEFT); // Image Descriptor

    if (s.status() != QDataStream::Ok) {
        return false;
    }

    for (int y = 0; y < img.height(); y++) {
        auto ptr = img.constScanLine(y);
        for (int x = 0; x < img.width(); x++) {
            s << *(ptr + x);
        }
        if (s.status() != QDataStream::Ok) {
            return false;
        }
    }

    return true;
}

bool TGAHandler::writeRGBA(const QImage &image)
{
    QDataStream s(device());
    s.setByteOrder(QDataStream::LittleEndian);

    QImage img(image);
    const bool hasAlpha = img.hasAlphaChannel();
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    auto cs = image.colorSpace();
    if (cs.isValid() && cs.colorModel() == QColorSpace::ColorModel::Cmyk && image.format() == QImage::Format_CMYK8888) {
        img = image.convertedToColorSpace(QColorSpace(QColorSpace::SRgb), QImage::Format_RGB32);
    } else if (hasAlpha && img.format() != QImage::Format_ARGB32) {
#else
    if (hasAlpha && img.format() != QImage::Format_ARGB32) {
#endif
        img = img.convertToFormat(QImage::Format_ARGB32);
    } else if (!hasAlpha && img.format() != QImage::Format_RGB32) {
        img = img.convertToFormat(QImage::Format_RGB32);
    }
    if (img.isNull()) {
        qCritical() << "TGAHandler::writeRGBA: image conversion to 32 bits failed!";
        return false;
    }
    static constexpr quint8 originTopLeft = TGA_ORIGIN_UPPER + TGA_ORIGIN_LEFT; // 0x20
    static constexpr quint8 alphaChannel8Bits = 0x08;

    for (int i = 0; i < 12; i++) {
        s << targaMagic[i];
    }

    // write header
    s << quint16(img.width()); // width
    s << quint16(img.height()); // height
    s << quint8(hasAlpha ? 32 : 24); // depth (24 bit RGB + 8 bit alpha)
    s << quint8(hasAlpha ? originTopLeft + alphaChannel8Bits : originTopLeft);   // top left image (0x20) + 8 bit alpha (0x8)

    if (s.status() != QDataStream::Ok) {
        return false;
    }

    for (int y = 0; y < img.height(); y++) {
        auto ptr = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x++) {
            auto color = *(ptr + x);
            s << quint8(qBlue(color));
            s << quint8(qGreen(color));
            s << quint8(qRed(color));
            if (hasAlpha) {
                s << quint8(qAlpha(color));
            }
        }
        if (s.status() != QDataStream::Ok) {
            return false;
        }
    }

    return true;
}

bool TGAHandler::supportsOption(ImageOption option) const
{
    if (option == QImageIOHandler::Size) {
        return true;
    }
    if (option == QImageIOHandler::ImageFormat) {
        return true;
    }
    return false;
}

QVariant TGAHandler::option(ImageOption option) const
{
    QVariant v;

    if (option == QImageIOHandler::Size) {
        auto&& header = d->m_header;
        if (IsSupported(header)) {
            v = QVariant::fromValue(QSize(header.width, header.height));
        } else if (auto dev = device()) {
            if (peekHeader(dev, header) && IsSupported(header)) {
                v = QVariant::fromValue(QSize(header.width, header.height));
            }
        }
    }

    if (option == QImageIOHandler::ImageFormat) {
        auto&& header = d->m_header;
        if (IsSupported(header)) {
            v = QVariant::fromValue(imageFormat(header));
        } else if (auto dev = device()) {
            if (peekHeader(dev, header) && IsSupported(header)) {
                v = QVariant::fromValue(imageFormat(header));
            }
        }
    }

    return v;
}

bool TGAHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("TGAHandler::canRead() called with no device");
        return false;
    }

    TgaHeader tga;
    if (!peekHeader(device, tga)) {
        qWarning("TGAHandler::canRead() error while reading the header");
        return false;
    }

    return IsSupported(tga);
}

QImageIOPlugin::Capabilities TGAPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "tga") {
        return Capabilities(CanRead | CanWrite);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && TGAHandler::canRead(device)) {
        cap |= CanRead;
    }
    if (device->isWritable()) {
        cap |= CanWrite;
    }
    return cap;
}

QImageIOHandler *TGAPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new TGAHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_tga_p.cpp"
