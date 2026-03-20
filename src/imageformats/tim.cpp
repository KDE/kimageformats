/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "tim_p.h"
#include "util_p.h"

#include <QIODevice>
#include <QImage>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LOG_TIMPLUGIN)
Q_LOGGING_CATEGORY(LOG_TIMPLUGIN, "kf.imageformats.plugins.tim", QtWarningMsg)

#define TYPE_4BPP 0 // never seen
#define TYPE_IDX_4BPP 8
#define TYPE_8BPP 1 // never seen
#define TYPE_IDX_8BPP 9
#define TYPE_16BPP 2
#define TYPE_24BPP 3

#define HEADER_SIZE 20

class TIMHeader
{
private:
    QByteArray m_rawHeader;

    quint16 ui16(quint8 c1, quint8 c2) const {
        return (quint16(c2) << 8) | quint16(c1);
    }

    quint32 ui32(quint8 c1, quint8 c2, quint8 c3, quint8 c4) const {
        return (quint32(c4) << 24) | (quint32(c3) << 16) | (quint32(c2) << 8) | quint32(c1);
    }

public:
    TIMHeader()
    {

    }

    quint32 type() const
    {
        if (m_rawHeader.size() < HEADER_SIZE) {
            return 0;
        }
        return ui32(m_rawHeader.at(4), m_rawHeader.at(5), m_rawHeader.at(6), m_rawHeader.at(7)) & 0xF;
    }

    quint32 offset() const
    {
        if (m_rawHeader.size() < HEADER_SIZE) {
            return 0;
        }
        auto o = quint32(HEADER_SIZE);
        auto t = type();
        if (t == TYPE_IDX_4BPP || t == TYPE_IDX_8BPP) { // indexed
            o += ui32(m_rawHeader.at(8), m_rawHeader.at(9), m_rawHeader.at(10), m_rawHeader.at(11));
        }
        return o;
    }

    bool isValid(quint32 size = 0) const
    {
        if (m_rawHeader.size() < HEADER_SIZE) {
            return false;
        }
        if (size == 0) {
            size = offset();
        }
        if (m_rawHeader.size() < size) {
            return false;
        }
        return (m_rawHeader.startsWith(QByteArray::fromRawData("\x10\x00\x00\x00", 4)));
    }

    bool isSupported() const
    {
        return format() != QImage::Format_Invalid;
    }

    qint32 width() const
    {
        auto strideLen = strideSize();
        auto t = type();
        if (t == TYPE_4BPP || t == TYPE_IDX_4BPP) {
            return strideLen * 2;
        }
        if (t == TYPE_8BPP || t == TYPE_IDX_8BPP) {
            return strideLen;
        }
        if (t == TYPE_24BPP) {
            return strideLen / 3;
        }
        return strideLen / 2;
    }

    qint32 height() const
    {
        auto o = offset();
        if (!isValid(o)) {
            return 0;
        }
        return qint32(ui16(m_rawHeader.at(o - 2), m_rawHeader.at(o - 1)));
    }

    QSize size() const
    {
        return QSize(width(), height());
    }

    QImage::Format format() const
    {
        auto t = type();
        if (t == TYPE_IDX_4BPP || t == TYPE_IDX_8BPP || t == TYPE_4BPP) {
            return QImage::Format_Indexed8;
        }
        if (t == TYPE_IDX_8BPP) {
            return QImage::Format_Grayscale8;
        }
        if (t == TYPE_16BPP) {
            return QImage::Format_RGB555;
        }
        if (t == TYPE_24BPP) {
            return QImage::Format_RGB888;
        }
        return QImage::Format_Invalid;
    }

    quint32 strideSize() const
    {
        auto o = offset();
        if (!isValid(o)) {
            return 0;
        }
        return ui16(m_rawHeader.at(o - 4), m_rawHeader.at(o - 3)) * 2;
    }

    qint32 paletteColors() const
    {
        if (this->format() != QImage::Format_Indexed8) {
            return 0;
        }
        return qint32(ui16(m_rawHeader.at(16), m_rawHeader.at(17)));
    }

    qint32 paletteCount() const
    {
        if (this->format() != QImage::Format_Indexed8) {
            return 0;
        }
        return qint32(ui16(m_rawHeader.at(18), m_rawHeader.at(19)));
    }

    QList<QRgb> palette() const
    {
        if (format() != QImage::Format_Indexed8) {
            return {};
        }

        // 4bpp without CLUT is treated  as indexed
        if (type() == TYPE_4BPP) {
            QList<QRgb> pal;
            for (auto i = 0; i < 16; ++i) {
                auto v = i * 17;
                pal << qRgb(v, v, v);
            }
            return pal;
        }

        // read the first paette only
        auto len = paletteColors();
        if (!isValid(HEADER_SIZE + len * 2)) {
            return {};
        }
        QList<QRgb> clut;
        for (auto i = 0; i < len; ++i) {
            auto v = ui16(m_rawHeader.at(HEADER_SIZE + i * 2), m_rawHeader.at(HEADER_SIZE + i * 2 + 1));
            // in some specs, the bit 15 is the alpha but with the image sample used, transparencies appear
            // where there shouldn't be any (so, disabled for now)
            clut << qRgba((v & 0x1F) * 255 / 31, ((v >> 5) & 0x1F) * 255 / 31, ((v >> 10) & 0x1F) * 255 / 31, 255);
        }
        return clut;
    }

    bool read(QIODevice *d)
    {
        m_rawHeader = d->read(HEADER_SIZE);
        if (m_rawHeader.size() != HEADER_SIZE) {
            return false;
        }
        auto o = offset() - HEADER_SIZE;
        if (o > kMaxQVectorSize - HEADER_SIZE) {
            return false;
        }
        m_rawHeader.append(d->read(o));
        return isValid();
    }

    bool peek(QIODevice *d)
    {
        m_rawHeader = d->peek(HEADER_SIZE);
        if (m_rawHeader.size() != HEADER_SIZE) {
            return false;
        }
        auto o = offset();
        if (o > kMaxQVectorSize - HEADER_SIZE) {
            return false;
        }
        if (o > m_rawHeader.size()) {
            m_rawHeader = d->peek(o);
        }
        return isValid();
    }

    bool jumpToImageData(QIODevice *d) const
    {
        if (d->isSequential()) {
            if (auto sz = std::max(offset() - quint32(m_rawHeader.size()), quint32())) {
                return d->read(sz).size() == sz;
            }
            return true;
        }
        return d->seek(offset());
    }
};

class TIMHandlerPrivate
{
public:
    TIMHandlerPrivate() {}
    ~TIMHandlerPrivate() {}

    TIMHeader m_header;
};

TIMHandler::TIMHandler()
    : QImageIOHandler()
    , d(new TIMHandlerPrivate)
{
}

bool TIMHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("tim");
        return true;
    }
    return false;
}

bool TIMHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_TIMPLUGIN) << "TIMHandler::canRead() called with no device";
        return false;
    }

    TIMHeader h;
    if (!h.peek(device)) {
        return false;
    }

    return h.isSupported();
}

bool TIMHandler::read(QImage *image)
{
    auto&& header = d->m_header;

    if (!header.read(device())) {
        qCWarning(LOG_TIMPLUGIN) << "TIMHandler::read() invalid header";
        return false;
    }

    auto img = imageAlloc(header.size(), header.format());
    if (img.isNull()) {
        qCWarning(LOG_TIMPLUGIN) << "TIMHandler::read() error while allocating the image";
        return false;
    }

    if (img.format() == QImage::Format_Indexed8) {
        auto pal = header.palette();
        if (pal.isEmpty()) {
            qCWarning(LOG_TIMPLUGIN) << "TIMHandler::read() error while reading the palette";
            return false;
        }
        img.setColorTable(pal);
    }

    auto d = device();
    if (!header.jumpToImageData(d)) {
        qCWarning(LOG_TIMPLUGIN) << "TIMHandler::read() error while seeking image data";
        return false;
    }

    auto size = std::min(img.bytesPerLine(), qsizetype(header.strideSize()));
    QByteArray tmpBuff;
    auto conv_4bpp = (header.type() == TYPE_4BPP || header.type() == TYPE_IDX_4BPP);
    if (conv_4bpp && size * 2 <= img.bytesPerLine()) {
        tmpBuff.resize(size);
    }
    for (auto y = 0, h = img.height(); y < h; ++y) {
        auto line = reinterpret_cast<char*>(img.scanLine(y));
        auto tbuf = tmpBuff.isEmpty() ? line : tmpBuff.data();
        if (d->read(tbuf, size) != size) {
            qCWarning(LOG_TIMPLUGIN) << "TIMHandler::read() error while reading image scanline";
            return false;
        }
        if (conv_4bpp) {
            for (auto x = 0, w = qint32(tmpBuff.size()); x < w; ++x) {
                auto &&v = tmpBuff.at(x);
                line[x * 2 + 1] = (v >> 4) & 0xF;
                line[x * 2] = v & 0xF;
            }
        }
    }

    if (img.format() == QImage::Format_RGB555) {
        img.rgbSwap();
    }

    *image = img;
    return true;
}

bool TIMHandler::supportsOption(ImageOption option) const
{
    if (option == QImageIOHandler::Size) {
        return true;
    }
    if (option == QImageIOHandler::ImageFormat) {
        return true;
    }
    return false;
}

QVariant TIMHandler::option(ImageOption option) const
{
    QVariant v;

    if (option == QImageIOHandler::Size) {
        auto&& h = d->m_header;
        if (h.isValid()) {
            v = QVariant::fromValue(h.size());
        } else if (auto d = device()) {
            if (h.peek(d)) {
                v = QVariant::fromValue(h.size());
            }
        }
    }

    if (option == QImageIOHandler::ImageFormat) {
        auto&& h = d->m_header;
        if (h.isValid()) {
            v = QVariant::fromValue(h.format());
        } else if (auto d = device()) {
            if (h.peek(d)) {
                v = QVariant::fromValue(h.format());
            }
        }
    }

    return v;
}

QImageIOPlugin::Capabilities TIMPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "tim") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && TIMHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *TIMPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new TIMHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_tim_p.cpp"
