/*
    KImageIO Routines to read (and perhaps in the future, write) images
    in the high dynamic range EXR format.

    SPDX-FileCopyrightText: 2003 Brad Hards <bradh@frogmouth.net>
    SPDX-FileCopyrightText: 2023 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/* *** EXR_USE_LEGACY_CONVERSIONS ***
 * If defined, the result image is an 8-bit RGB(A) converted
 * without icc profiles. Otherwise, a 16-bit images is generated.
 * NOTE: The use of legacy conversions are discouraged due to
 *       imprecise image result.
 */
//#define EXR_USE_LEGACY_CONVERSIONS // default commented -> you should define it in your cmake file

/* *** EXR_ALLOW_LINEAR_COLORSPACE ***
 * If defined, the linear data is kept and it is the display program that
 * must convert to the monitor profile. Otherwise the data is converted to sRGB
 * to accommodate programs that do not support color profiles.
 * NOTE: If EXR_USE_LEGACY_CONVERSIONS is active, this is ignored.
 */
//#define EXR_ALLOW_LINEAR_COLORSPACE // default: commented -> you should define it in your cmake file

#include "exr_p.h"
#include "util_p.h"

#include <IexThrowErrnoExc.h>
#include <ImathBox.h>
#include <ImfArray.h>
#include <ImfBoxAttribute.h>
#include <ImfChannelListAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfConvert.h>
#include <ImfFloatAttribute.h>
#include <ImfInputFile.h>
#include <ImfInt64.h>
#include <ImfIntAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfRgbaFile.h>
#include <ImfStandardAttributes.h>
#include <ImfStringAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfVersion.h>

#include <iostream>

#include <QColorSpace>
#include <QDataStream>
#include <QDebug>
#include <QFloat16>
#include <QImage>
#include <QImageIOPlugin>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QTimeZone>
#endif

// Allow the code to works on all QT versions supported by KDE
// project (Qt 5.15 and Qt 6.x) to easy backports fixes.
#if (QT_VERSION_MAJOR >= 6) && !defined(EXR_USE_LEGACY_CONVERSIONS)
// If uncommented, the image is rendered in a float16 format, the result is very precise
#define EXR_USE_QT6_FLOAT_IMAGE // default uncommented
#endif

class K_IStream : public Imf::IStream
{
public:
    K_IStream(QIODevice *dev, const QByteArray &fileName)
        : IStream(fileName.data())
        , m_dev(dev)
    {
    }

    bool read(char c[], int n) override;
#if OPENEXR_VERSION_MAJOR > 2
    uint64_t tellg() override;
    void seekg(uint64_t pos) override;
#else
    Imf::Int64 tellg() override;
    void seekg(Imf::Int64 pos) override;
#endif
    void clear() override;

private:
    QIODevice *m_dev;
};

bool K_IStream::read(char c[], int n)
{
    qint64 result = m_dev->read(c, n);
    if (result > 0) {
        return true;
    } else if (result == 0) {
        throw Iex::InputExc("Unexpected end of file");
    } else { // negative value {
        Iex::throwErrnoExc("Error in read", result);
    }
    return false;
}

#if OPENEXR_VERSION_MAJOR > 2
uint64_t K_IStream::tellg()
#else
Imf::Int64 K_IStream::tellg()
#endif
{
    return m_dev->pos();
}

#if OPENEXR_VERSION_MAJOR > 2
void K_IStream::seekg(uint64_t pos)
#else
void K_IStream::seekg(Imf::Int64 pos)
#endif
{
    m_dev->seek(pos);
}

void K_IStream::clear()
{
    // TODO
}

#ifdef EXR_USE_LEGACY_CONVERSIONS
// source: https://openexr.com/en/latest/ReadingAndWritingImageFiles.html
inline unsigned char gamma(float x)
{
    x = std::pow(5.5555f * std::max(0.f, x), 0.4545f) * 84.66f;
    return (unsigned char)qBound(0.f, x, 255.f);
}
inline QRgb RgbaToQrgba(struct Imf::Rgba &imagePixel)
{
    return qRgba(gamma(float(imagePixel.r)),
                 gamma(float(imagePixel.g)),
                 gamma(float(imagePixel.b)),
                 (unsigned char)(qBound(0.f, imagePixel.a * 255.f, 255.f) + 0.5f));
}
#endif

EXRHandler::EXRHandler()
{
}

bool EXRHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("exr");
        return true;
    }
    return false;
}

bool EXRHandler::read(QImage *outImage)
{
    try {
        int width;
        int height;

        K_IStream istr(device(), QByteArray());
        Imf::RgbaInputFile file(istr);
        Imath::Box2i dw = file.dataWindow();
        bool isRgba = file.channels() & Imf::RgbaChannels::WRITE_A;

        width = dw.max.x - dw.min.x + 1;
        height = dw.max.y - dw.min.y + 1;

#if defined(EXR_USE_LEGACY_CONVERSIONS)
        QImage image = imageAlloc(width, height, isRgba ? QImage::Format_ARGB32 : QImage::Format_RGB32);
#elif defined(EXR_USE_QT6_FLOAT_IMAGE)
        QImage image = imageAlloc(width, height, isRgba ? QImage::Format_RGBA16FPx4 : QImage::Format_RGBX16FPx4);
#else
        QImage image = imageAlloc(width, height, isRgba ? QImage::Format_RGBA64 : QImage::Format_RGBX64);
#endif
        if (image.isNull()) {
            qWarning() << "Failed to allocate image, invalid size?" << QSize(width, height);
            return false;
        }

        // set some useful metadata
        auto &&h = file.header();
        if (auto comments = h.findTypedAttribute<Imf::StringAttribute>("comments")) {
            image.setText(QStringLiteral("Comment"), QString::fromStdString(comments->value()));
        }
        if (auto owner = h.findTypedAttribute<Imf::StringAttribute>("owner")) {
            image.setText(QStringLiteral("Owner"), QString::fromStdString(owner->value()));
        }
        if (auto capDate = h.findTypedAttribute<Imf::StringAttribute>("capDate")) {
            float off = 0;
            if (auto utcOffset = h.findTypedAttribute<Imf::FloatAttribute>("utcOffset")) {
                off = utcOffset->value();
            }
            auto dateTime = QDateTime::fromString(QString::fromStdString(capDate->value()), QStringLiteral("yyyy:MM:dd HH:mm:ss"));
            if (dateTime.isValid()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                dateTime.setTimeZone(QTimeZone::fromSecondsAheadOfUtc(off));
#else
                dateTime.setOffsetFromUtc(off);
#endif
                image.setText(QStringLiteral("Date"), dateTime.toString(Qt::ISODate));
            }
        }
        if (auto xDensity = h.findTypedAttribute<Imf::FloatAttribute>("xDensity")) {
            float par = 1;
            if (auto pixelAspectRatio = h.findTypedAttribute<Imf::FloatAttribute>("pixelAspectRatio")) {
                par = pixelAspectRatio->value();
            }
            image.setDotsPerMeterX(qRound(xDensity->value() * 100.0 / 2.54));
            image.setDotsPerMeterY(qRound(xDensity->value() * par * 100.0 / 2.54));
        }

        Imf::Array<Imf::Rgba> pixels;
        pixels.resizeErase(width);

        // somehow copy pixels into image
        for (int y = 0; y < height; ++y) {
            auto my = dw.min.y + y;
            if (my <= dw.max.y) { // paranoia check
                file.setFrameBuffer(&pixels[0] - dw.min.x - qint64(my) * width, 1, width);
                file.readPixels(my, my);

#if defined(EXR_USE_LEGACY_CONVERSIONS)
                auto scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    *(scanLine + x) = RgbaToQrgba(pixels[x]);
                }
#elif defined(EXR_USE_QT6_FLOAT_IMAGE)
                auto scanLine = reinterpret_cast<qfloat16 *>(image.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    auto xcs = x * 4;
                    *(scanLine + xcs) = qfloat16(qBound(0.f, float(pixels[x].r), 1.f));
                    *(scanLine + xcs + 1) = qfloat16(qBound(0.f, float(pixels[x].g), 1.f));
                    *(scanLine + xcs + 2) = qfloat16(qBound(0.f, float(pixels[x].b), 1.f));
                    *(scanLine + xcs + 3) = qfloat16(isRgba ? qBound(0.f, float(pixels[x].a), 1.f) : 1.f);
                }
#else
                auto scanLine = reinterpret_cast<QRgba64 *>(image.scanLine(y));
                for (int x = 0; x < width; ++x) {
                    *(scanLine + x) = QRgba64::fromRgba64(quint16(qBound(0.f, float(pixels[x].r) * 65535.f + 0.5f, 65535.f)),
                                                          quint16(qBound(0.f, float(pixels[x].g) * 65535.f + 0.5f, 65535.f)),
                                                          quint16(qBound(0.f, float(pixels[x].b) * 65535.f + 0.5f, 65535.f)),
                                                          isRgba ? quint16(qBound(0.f, float(pixels[x].a) * 65535.f + 0.5f, 65535.f)) : quint16(65535));
                }
#endif
            }
        }

        // final color operations
#ifndef EXR_USE_LEGACY_CONVERSIONS
        image.setColorSpace(QColorSpace(QColorSpace::SRgbLinear));
#ifndef EXR_ALLOW_LINEAR_COLORSPACE
        image.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
#endif // !EXR_ALLOW_LINEAR_COLORSPACE
#endif // !EXR_USE_LEGACY_CONVERSIONS

        *outImage = image;

        return true;
    } catch (const std::exception &exc) {
        //      qDebug() << exc.what();
        return false;
    }
}

bool EXRHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("EXRHandler::canRead() called with no device");
        return false;
    }

    const QByteArray head = device->peek(4);

    return Imf::isImfMagic(head.data());
}

QImageIOPlugin::Capabilities EXRPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "exr") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && EXRHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *EXRPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new EXRHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_exr_p.cpp"
