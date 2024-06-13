/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2024 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/*
 * See also: https://www.pauldebevec.com/Research/HDR/PFM/
 */

#include "pfm_p.h"
#include "util_p.h"

#include <QColorSpace>
#include <QDataStream>
#include <QIODevice>
#include <QImage>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LOG_PFMPLUGIN)
Q_LOGGING_CATEGORY(LOG_PFMPLUGIN, "kf.imageformats.plugins.pfm", QtWarningMsg)

class PfmHeader
{
private:
    /*!
     * \brief m_bw True if grayscale.
     */
    bool m_bw;

    /*!
     * \brief m_ps True if saved by Photoshop (Photoshop variant).
     *
     * When \a false the format of the header is the following (GIMP):
     * [type]
     * [xres] [yres]
     * [byte_order]
     *
     * When \a true the format of the header is the following (Photoshop):
     * [type]
     * [xres]
     * [yres]
     * [byte_order]
     */
    bool m_ps;

    /*!
     * \brief m_width The image width.
     */
    qint32 m_width;

    /*!
     * \brief m_height The image height.
     */
    qint32 m_height;

    /*!
     * \brief m_byteOrder The image byte orger.
     */
    QDataStream::ByteOrder m_byteOrder;

public:
    PfmHeader() :
        m_bw(false),
        m_ps(false),
        m_width(0),
        m_height(0),
        m_byteOrder(QDataStream::BigEndian)
    {

    }

    bool isValid() const
    {
        return (m_width > 0 && m_height > 0);
    }

    bool isBlackAndWhite() const
    {
        return m_bw;
    }

    bool isPhotoshop() const
    {
        return m_ps;
    }

    qint32 width() const
    {
        return m_width;
    }

    qint32 height() const
    {
        return m_height;
    }

    QSize size() const
    {
        return QSize(m_width, m_height);
    }

    QDataStream::ByteOrder byteOrder() const
    {
        return m_byteOrder;
    }

    QImage::Format format() const
    {
        if (isValid()) {
            return isBlackAndWhite() ? QImage::Format_Grayscale16 : QImage::Format_RGBX32FPx4;
        }
        return QImage::Format_Invalid;
    }

    bool read(QIODevice *d)
    {
        auto pf = d->read(3);
        if (pf == QByteArray("PF\n")) {
            m_bw = false;
        } else if (pf == QByteArray("Pf\n")) {
            m_bw = true;
        } else {
            return false;
        }
        auto wh = QString::fromLatin1(d->readLine(128));
        auto list = wh.split(QStringLiteral(" "));
        if (list.size() == 1) {
            m_ps = true; // try for Photoshop version
            list << QString::fromLatin1(d->readLine(128));
        }
        if (list.size() != 2) {
            return false;
        }
        auto ok_o = false;
        auto ok_w = false;
        auto ok_h = false;
        auto o = QString::fromLatin1(d->readLine(128)).toDouble(&ok_o);
        auto w = list.first().toInt(&ok_w);
        auto h = list.last().toInt(&ok_h);
        if (!ok_o || !ok_w || !ok_h || o == 0) {
            return false;
        }
        m_width = w;
        m_height = h;
        m_byteOrder = o > 0 ? QDataStream::BigEndian : QDataStream::LittleEndian;
        return isValid();
    }

    bool peek(QIODevice *d)
    {
        d->startTransaction();
        auto ok = read(d);
        d->rollbackTransaction();
        return ok;
    }
} ;

PFMHandler::PFMHandler()
{
}

bool PFMHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("pfm");
        return true;
    }
    return false;
}

bool PFMHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_PFMPLUGIN) << "PFMHandler::canRead() called with no device";
        return false;
    }

    PfmHeader h;
    if (!h.peek(device)) {
        return false;
    }

    return h.isValid();
}

bool PFMHandler::read(QImage *image)
{
    PfmHeader header;

    if (!header.read(device())) {
        qCWarning(LOG_PFMPLUGIN) << "PFMHandler::read() invalid header";
        return false;
    }

    QDataStream s(device());
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s.setByteOrder(header.byteOrder());

    auto img = imageAlloc(header.size(), header.format());
    if (img.isNull()) {
        qCWarning(LOG_PFMPLUGIN) << "PFMHandler::read() error while allocating the image";
        return false;
    }

    for (auto y = 0, h = img.height(); y < h; ++y) {
        float f;
        if (header.isBlackAndWhite()) {
            auto line = reinterpret_cast<quint16*>(img.scanLine(header.isPhotoshop() ? y : h - y - 1));
            for (auto x = 0, w = img.width(); x < w; ++x) {
                s >> f;
                // QColorSpace does not handle gray linear profile, so I have to convert to non-linear
                f = f < 0.0031308f ? (f * 12.92f) : (1.055 * std::pow(f, 1.0 / 2.4) - 0.055);
                line[x] = quint16(std::clamp(f, float(0), float(1)) * std::numeric_limits<quint16>::max() + float(0.5));

                if (s.status() != QDataStream::Ok) {
                    qCWarning(LOG_PFMPLUGIN) << "PFMHandler::read() detected corrupted data";
                    return false;
                }
            }
        } else {
            auto line = reinterpret_cast<float*>(img.scanLine(header.isPhotoshop() ? y : h - y - 1));
            for (auto x = 0, n = img.width() * 4; x < n; x += 4) {
                s >> f;
                line[x] = std::clamp(f, float(0), float(1));
                s >> f;
                line[x + 1] = std::clamp(f, float(0), float(1));
                s >> f;
                line[x + 2] = std::clamp(f, float(0), float(1));
                line[x + 3] = float(1);

                if (s.status() != QDataStream::Ok) {
                    qCWarning(LOG_PFMPLUGIN) << "PFMHandler::read() detected corrupted data";
                    return false;
                }
            }
        }
    }

    if (!header.isBlackAndWhite()) {
        img.setColorSpace(QColorSpace(QColorSpace::SRgbLinear));
    }

    *image = img;
    return true;
}

bool PFMHandler::supportsOption(ImageOption option) const
{
    if (option == QImageIOHandler::Size) {
        return true;
    }
    if (option == QImageIOHandler::ImageFormat) {
        return true;
    }
    if (option == QImageIOHandler::Endianness) {
        return true;
    }
    return false;
}

QVariant PFMHandler::option(ImageOption option) const
{
    QVariant v;

    if (option == QImageIOHandler::Size) {
        if (auto d = device()) {
            PfmHeader h;
            if (h.peek(d)) {
                v = QVariant::fromValue(h.size());
            }
        }
    }

    if (option == QImageIOHandler::ImageFormat) {
        if (auto d = device()) {
            PfmHeader h;
            if (h.peek(d)) {
                v = QVariant::fromValue(h.format());
            }
        }
    }

    if (option == QImageIOHandler::Endianness) {
        if (auto d = device()) {
            PfmHeader h;
            if (h.peek(d)) {
                v = QVariant::fromValue(h.byteOrder());
            }
        }
    }

    return v;
}

QImageIOPlugin::Capabilities PFMPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "pfm") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && PFMHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *PFMPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new PFMHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_pfm_p.cpp"
