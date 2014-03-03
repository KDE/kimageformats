/**
 * Softimage PIC support for QImage
 * Copyright 1998 Halfdan Ingvarsson
 * Copyright 2007 Ruben Lopez <r.lopez@bren.es>
 * Copyright 2014 Alex Merry <alex.merry@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------------
 */

/* This code is based on the GIMP-PIC plugin by Halfdan Ingvarsson,
 * and relicensed from GPL to LGPL to accommodate the KDE licensing policy
 * with his permission.
 */

#include "pic.h"

#include <QDataStream>
#include <QDebug>
#include <QImage>
#include <QVariant>
#include <qendian.h>
#include <algorithm>
#include <functional>

/**
 * Reads a PIC file header from a data stream.
 *
 * @param s         The data stream to read from.
 * @param channels  Where the read header will be stored.
 * @returns  @p s
 *
 * @relates PicHeader
 */
static QDataStream &operator>> (QDataStream &s, PicHeader &header)
{
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s >> header.magic;
    s >> header.version;

    // the comment should be truncated to the first null byte
    char comment[81] = {};
    s.readRawData(comment, 80);
    header.comment = QByteArray(comment);

    header.id.resize(4);
    s.readRawData(header.id.data(), 4);

    s >> header.width;
    s >> header.height;
    s >> header.ratio;
    qint16 fields;
    s >> fields;
    header.fields = static_cast<PicFields>(fields);
    qint16 pad;
    s >> pad;
    return s;
}

/**
 * Writes a PIC file header to a data stream.
 *
 * @param s         The data stream to write to.
 * @param channels  The header to write.
 * @returns  @p s
 *
 * @relates PicHeader
 */
static QDataStream &operator<< (QDataStream &s, const PicHeader &header)
{
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
    s << header.magic;
    s << header.version;

    char comment[80] = {};
    strncpy(comment, header.comment.constData(), sizeof(comment));
    s.writeRawData(comment, sizeof(comment));

    char id[4] = {};
    strncpy(id, header.id.constData(), sizeof(id));
    s.writeRawData(id, sizeof(id));

    s << header.width;
    s << header.height;
    s << header.ratio;
    s << quint16(header.fields);
    s << quint16(0);
    return s;
}

/**
 * Reads a series of channel descriptions from a data stream.
 *
 * If the stream contains more than 8 channel descriptions, the status of @p s
 * will be set to QDataStream::ReadCorruptData (note that more than 4 channels
 * - one for each component - does not really make sense anyway).
 *
 * @param s         The data stream to read from.
 * @param channels  The location to place the read channel descriptions; any
 *                  existing entries will be cleared.
 * @returns  @p s
 *
 * @relates PicChannel
 */
static QDataStream &operator>> (QDataStream &s, QList<PicChannel> &channels)
{
    const unsigned maxChannels = 8;
    unsigned count = 0;
    quint8 chained = 1;
    channels.clear();
    while (chained && count < maxChannels && s.status() == QDataStream::Ok) {
        PicChannel channel;
        s >> chained;
        s >> channel.size;
        quint8 encoding;
        s >> encoding;
        channel.encoding = PicChannelEncoding(encoding);
        s >> channel.code;
        channels << channel;
        ++count;
    }
    if (chained) {
        // too many channels!
        s.setStatus(QDataStream::ReadCorruptData);
    }
    return s;
}

/**
 * Writes a series of channel descriptions to a data stream.
 *
 * Note that the corresponding read operation will not read more than 8 channel
 * descriptions, although there should be no reason to have more than 4 channels
 * anyway.
 *
 * @param s         The data stream to write to.
 * @param channels  The channel descriptions to write.
 * @returns  @p s
 *
 * @relates PicChannel
 */
static QDataStream &operator<< (QDataStream &s, const QList<PicChannel> &channels)
{
    Q_ASSERT(channels.size() > 0);
    for (int i = 0; i < channels.size() - 1; ++i) {
        s << quint8(1); // chained
        s << channels[i].size;
        s << quint8(channels[i].encoding);
        s << channels[i].code;
    }
    s << quint8(0); // chained
    s << channels.last().size;
    s << quint8(channels.last().encoding);
    s << channels.last().code;
    return s;
}

/**
 * Decodes data written in mixed run-length encoding format.
 *
 * This is intended to be used with lambda functions.
 *
 * Note that this functions expects that, at the current location in @p stream,
 * exactly @p length items have been encoded as a unit (and so it will not be
 * partway through a run when it has decoded @p length items). If this is not
 * the case, it will return @c false.
 *
 * @param stream      The stream to read the data from.
 * @param data        The location to write the data.
 * @param length      The number of items to read.
 * @param readItem    A function that takes a QDataStream reference and reads a
 *                    single item.
 * @param updateItem  A function that takes an item from @p data and an item
 *                    read by @p readItem and produces the item that should be
 *                    written to @p data.
 *
 * @returns @c true if @p length items in mixed RLE were successfully read
 *          into @p data, @c false otherwise.
 */
template<typename Item, typename Func1, typename Func2>
static bool decodeMixedRLEData(QDataStream &stream,
                               Item *data,
                               quint16 length,
                               Func1 readItem,
                               Func2 updateItem)
{
    unsigned offset = 0; // in data
    while (offset < length) {
        unsigned remaining = length - offset;
        quint8 count1;
        stream >> count1;

        if (count1 >= 128u) {
            unsigned length;
            if (count1 == 128u) {
                // If the value is exactly 128, it means that it is more than
                // 127 repetitions
                quint16 count2;
                stream >> count2;
                length = count2;
            } else {
                // If last bit is 1, then it is 2 to 127 repetitions
                length = count1 - 127u;
            }
            if (length > remaining) {
                qDebug() << "Row overrun:" << length << ">" << remaining;
                return false;
            }
            Item item = readItem(stream);
            for (unsigned i = offset; i < offset + length; ++i) {
                data[i] = updateItem(data[i], item);
            }
            offset += length;
        } else {
            // No repetitions
            unsigned length = count1 + 1u;
            if (length > remaining) {
                qDebug() << "Row overrun:" << length << ">" << remaining;
                return false;
            }
            for (unsigned i = offset; i < offset + length; ++i) {
                Item item = readItem(stream);
                data[i] = updateItem(data[i], item);
            }
            offset += length;
        }
    }
    if (stream.status() != QDataStream::Ok) {
        qDebug() << "DataStream status was" << stream.status();;;;
    }
    return stream.status() == QDataStream::Ok;
}

static bool readRow(QDataStream &stream, QRgb *row, quint16 width, QList<PicChannel> channels)
{
    Q_FOREACH(const PicChannel &channel, channels) {
        auto readPixel = [&] (QDataStream &str) -> QRgb {
            quint8 red = 0;
            if (channel.code & RED) {
                str >> red;
            }
            quint8 green = 0;
            if (channel.code & GREEN) {
                str >> green;
            }
            quint8 blue = 0;
            if (channel.code & BLUE) {
                str >> blue;
            }
            quint8 alpha = 0;
            if (channel.code & ALPHA) {
                str >> alpha;
            }
            return qRgba(red, green, blue, alpha);
        };
        auto updatePixel = [&] (QRgb oldPixel, QRgb newPixel) -> QRgb {
            return qRgba(
                qRed((channel.code & RED) ? newPixel : oldPixel),
                qGreen((channel.code & GREEN) ? newPixel : oldPixel),
                qBlue((channel.code & BLUE) ? newPixel : oldPixel),
                qAlpha((channel.code & ALPHA) ? newPixel : oldPixel));
        };
        if (channel.encoding == MixedRLE) {
            if (!decodeMixedRLEData(stream, row, width, readPixel, updatePixel)) {
                qDebug() << "decodeMixedRLEData failed";
                return false;
            }
        } else if (channel.encoding == Uncompressed) {
            for (quint16 i = 0; i < width; ++i) {
                QRgb pixel = readPixel(stream);
                row[i] = updatePixel(row[i], pixel);
            }
        } else {
            // unknown encoding
            qDebug() << "Unknown encoding";
            return false;
        }
    }
    if (stream.status() != QDataStream::Ok) {
        qDebug() << "DataStream status was" << stream.status();
    }
    return stream.status() == QDataStream::Ok;
}

/**
 * Encodes data in mixed run-length encoding format.
 *
 * This is intended to be used with lambda functions.
 *
 * @param stream      The stream to write the data to.
 * @param data        The data to be written.
 * @param length      The number of items to write.
 * @param itemsEqual  A function that takes two items and returns whether
 *                    @p writeItem would write them identically.
 * @param writeItem   A function that takes a QDataStream reference and an item
 *                    and writes the item to the data stream.
 */
template<typename Item, typename Func1, typename Func2>
static void encodeMixedRLEData(QDataStream &stream, const Item *data, unsigned length, Func1 itemsEqual, Func2 writeItem)
{
    unsigned offset = 0;
    while (offset < length) {
        const Item *chunkStart = data + offset;
        unsigned maxChunk = qMin(length - offset, 65535u);

        const Item *chunkEnd = chunkStart + 1;
        quint16 chunkLength = 1;
        while (chunkLength < maxChunk && itemsEqual(*chunkStart, *chunkEnd)) {
            ++chunkEnd;
            ++chunkLength;
        }

        if (chunkLength > 127) {
            // Sequence of > 127 identical pixels
            stream << quint8(128);
            stream << quint16(chunkLength);
            writeItem(stream, *chunkStart);
        } else if (chunkLength > 1) {
            // Sequence of < 128 identical pixels
            stream << quint8(chunkLength + 127);
            writeItem(stream, *chunkStart);
        } else {
            // find a string of up to 128 values, each different from the one
            // that follows it
            if (maxChunk > 128) {
                maxChunk = 128;
            }
            chunkLength = 1;
            chunkEnd = chunkStart + 1;
            while (chunkLength < maxChunk &&
                    (chunkLength + 1 == maxChunk ||
                     !itemsEqual(*chunkEnd, *(chunkEnd+1))))
            {
                ++chunkEnd;
                ++chunkLength;
            }
            stream << quint8(chunkLength - 1);
            for (unsigned i = 0; i < chunkLength; ++i) {
                writeItem(stream, *(chunkStart + i));
            }
        }
        offset += chunkLength;
    }
}

bool SoftimagePICHandler::canRead() const
{
    if (!SoftimagePICHandler::canRead(device())) {
        return false;
    }
    setFormat("pic");
    return true;
}

bool SoftimagePICHandler::read(QImage *image)
{
    if (!readChannels()) {
        return false;
    }

    QImage::Format fmt = QImage::Format_RGB32;
    Q_FOREACH(const PicChannel &channel, m_channels) {
        if (channel.size != 8) {
            // we cannot read images that do not come in bytes
            qDebug() << "Channel size was" << channel.size;
            m_state = Error;
            return false;
        }
        if (channel.code & ALPHA) {
            fmt = QImage::Format_ARGB32;
        }
    }

    QImage img(m_header.width, m_header.height, fmt);
    img.fill(qRgb(0,0,0));

    for (int y = 0; y < m_header.height; y++) {
        QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));
        if (!readRow(m_dataStream, row, m_header.width, m_channels)) {
            qDebug() << "readRow failed";
            m_state = Error;
            return false;
        }
    }

    *image = img;
    m_state = Ready;

    return true;
}

bool SoftimagePICHandler::write(const QImage &_image)
{
    bool alpha = _image.hasAlphaChannel();
    const QImage image = _image.convertToFormat(
            alpha ? QImage::Format_ARGB32
                  : QImage::Format_RGB32);

    if (image.width() < 0 || image.height() < 0) {
        qDebug() << "Image size invalid:" << image.width() << image.height();
        return false;
    }
    if (image.width() > 65535 || image.height() > 65535) {
        qDebug() << "Image too big:" << image.width() << image.height();
        // there are only two bytes for each dimension
        return false;
    }

    QDataStream stream(device());

    stream << PicHeader(image.width(), image.height(), m_description);

    PicChannelEncoding encoding = m_compression ? MixedRLE : Uncompressed;
    QList<PicChannel> channels;
    channels << PicChannel(encoding, RED | GREEN | BLUE);
    if (alpha) {
        channels << PicChannel(encoding, ALPHA);
    }
    stream << channels;

    for (int r = 0; r < image.height(); r++) {
        const QRgb *row = reinterpret_cast<const QRgb*>(image.scanLine(r));

        /* Write the RGB part of the scanline */
        auto rgbEqual = [] (QRgb p1, QRgb p2) -> bool {
            return qRed(p1) == qRed(p2) &&
                   qGreen(p1) == qGreen(p2) &&
                   qBlue(p1) == qBlue(p2);
        };
        auto writeRgb = [] (QDataStream &str, QRgb pixel) -> void {
            str << quint8(qRed(pixel))
                << quint8(qGreen(pixel))
                << quint8(qBlue(pixel));
        };
        if (m_compression) {
            encodeMixedRLEData(stream, row, image.width(), rgbEqual, writeRgb);
        } else {
            for (int i = 0; i < image.width(); ++i) {
                writeRgb(stream, row[i]);
            }
        }

        /* Write the alpha channel */
        if (alpha) {
            auto alphaEqual = [] (QRgb p1, QRgb p2) -> bool {
                return qAlpha(p1) == qAlpha(p2);
            };
            auto writeAlpha = [] (QDataStream &str, QRgb pixel) -> void {
                str << quint8(qAlpha(pixel));
            };
            if (m_compression) {
                encodeMixedRLEData(stream, row, image.width(), alphaEqual, writeAlpha);
            } else {
                for (int i = 0; i < image.width(); ++i) {
                    writeAlpha(stream, row[i]);
                }
            }
        }
    }
    return stream.status() == QDataStream::Ok;
}

bool SoftimagePICHandler::canRead(QIODevice *device)
{
    char data[4];
    if (device->peek(data, 4) != 4) {
        return false;
    }
    return qFromBigEndian<qint32>(reinterpret_cast<uchar*>(data)) == PIC_MAGIC_NUMBER;
}

bool SoftimagePICHandler::readHeader()
{
    if (m_state == Ready) {
        m_state = Error;
        m_dataStream.setDevice(device());
        m_dataStream >> m_header;
        if (m_header.isValid() && m_dataStream.status() == QDataStream::Ok) {
            m_state = ReadHeader;
        }
    }
    return m_state != Error;
}

bool SoftimagePICHandler::readChannels()
{
    readHeader();
    if (m_state == ReadHeader) {
        m_state = Error;
        m_dataStream >> m_channels;
        if (m_dataStream.status() == QDataStream::Ok) {
            m_state = ReadChannels;
        }
    }
    return m_state != Error;
}

void SoftimagePICHandler::setOption(ImageOption option, const QVariant &value)
{
    switch (option) {
        case CompressionRatio:
            m_compression = value.toBool();
            break;
        case Description: {
            m_description.clear();
            QStringList entries = value.toString().split(QStringLiteral("\n\n"));
            Q_FOREACH(const QString entry, entries) {
                if (entry.startsWith(QStringLiteral("Description: "))) {
                    m_description = entry.mid(13).simplified().toUtf8();
                }
            }
            break;
        }
        default:
            break;
    }
}

QVariant SoftimagePICHandler::option(ImageOption option) const
{
    const_cast<SoftimagePICHandler*>(this)->readHeader();
    switch (option) {
        case Size:
            if (const_cast<SoftimagePICHandler*>(this)->readHeader()) {
                return QSize(m_header.width, m_header.height);
            } else {
                return QVariant();
            }
        case CompressionRatio:
            return m_compression;
        case Description:
            if (const_cast<SoftimagePICHandler*>(this)->readHeader()) {
                QString descStr = QString::fromUtf8(m_header.comment);
                if (!descStr.isEmpty()) {
                    return QString(QStringLiteral("Description: ") +
                           descStr +
                           QStringLiteral("\n\n"));
                }
            }
            return QString();
        case ImageFormat:
            if (const_cast<SoftimagePICHandler*>(this)->readChannels()) {
                Q_FOREACH (const PicChannel &channel, m_channels) {
                    if (channel.code & ALPHA) {
                        return QImage::Format_ARGB32;
                    }
                }
                return QImage::Format_RGB32;
            }
            return QVariant();
        default:
            return QVariant();
    }
}

bool SoftimagePICHandler::supportsOption(ImageOption option) const
{
    return (option == CompressionRatio ||
            option == Description ||
            option == ImageFormat ||
            option == Size);
}

QImageIOPlugin::Capabilities SoftimagePICPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "pic") {
        return Capabilities(CanRead | CanWrite);
    }
    if (!format.isEmpty()) {
        return 0;
    }
    if (!device->isOpen()) {
        return 0;
    }

    Capabilities cap;
    if (device->isReadable() && SoftimagePICHandler::canRead(device)) {
        cap |= CanRead;
    }
    if (device->isWritable()) {
        cap |= CanWrite;
    }
    return cap;
}

QImageIOHandler *SoftimagePICPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new SoftimagePICHandler();
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}
