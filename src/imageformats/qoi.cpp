/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2023 Ernest Gupik <ernestgupik@wp.pl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "qoi.h"

#include "util_p.h"
#include <QColorSpace>
#include <QFile>
#include <QIODevice>
#include <QImage>

namespace // Private
{

#define QOI_OP_INDEX 0x00 /* 00xxxxxx */
#define QOI_OP_DIFF 0x40 /* 01xxxxxx */
#define QOI_OP_LUMA 0x80 /* 10xxxxxx */
#define QOI_OP_RUN 0xc0 /* 11xxxxxx */
#define QOI_OP_RGB 0xfe /* 11111110 */
#define QOI_OP_RGBA 0xff /* 11111111 */
#define QOI_MASK_2 0xc0 /* 11000000 */

#define QOI_MAGIC (((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 | ((unsigned int)'i') << 8 | ((unsigned int)'f'))
#define QOI_HEADER_SIZE 14

struct QoiHeader {
    quint32 MagicNumber;
    quint32 Width;
    quint32 Height;
    quint8 Channels;
    quint8 Colorspace;
};

struct Px {
    quint8 r;
    quint8 g;
    quint8 b;
    quint8 a;
};

static QDataStream &operator>>(QDataStream &s, QoiHeader &head)
{
    s >> head.MagicNumber;
    s >> head.Width;
    s >> head.Height;
    s >> head.Channels;
    s >> head.Colorspace;
    return s;
}

static bool IsSupported(const QoiHeader &head)
{
    // Check magic number
    if (head.MagicNumber != QOI_MAGIC) {
        return false;
    }
    // Check if the header is a valid QOI header
    if (head.Width == 0 || head.Height == 0 || head.Channels < 3 || head.Colorspace > 1) {
        return false;
    }
    return true;
}

static int QoiHash(const Px &px)
{
    return px.r * 3 + px.g * 5 + px.b * 7 + px.a * 11;
}

static bool LoadQOI(QDataStream &s, const QoiHeader &qoi, QImage &img)
{
    s.device()->seek(QOI_HEADER_SIZE);

    Px index[64] = {Px{
        0,
        0,
        0,
        0,
    }};

    Px px = Px{
        0,
        0,
        0,
        255,
    };

    if (qoi.Width * qoi.Height > UINT64_MAX / qoi.Channels) {
        return false;
    }

    quint64 px_len = quint64(qoi.Width) * qoi.Height * qoi.Channels;
    quint32 chunks_len = s.device()->size() - 8; // 8 is the size of the QOI padding
    quint32 run = 0;
    quint32 p = 0;

    if (px_len > kMaxQVectorSize) {
        return false;
    }

    QVector<quint8> input(px_len);

    int i = 0;
    while (!s.atEnd() && i < input.size()) {
        s >> input[i];
        i++;
    }

    // Allocate image
    img = QImage(qoi.Width, qoi.Height, QImage::Format_ARGB32);

    if (img.isNull()) {
        return false;
    }

    // Set the image colorspace based on the qoi.Colorspace value
    // As per specification: 0 = sRGB with linear alpha, 1 = all channels linear
    switch (qoi.Colorspace) {
    case 0:
        img.setColorSpace(QColorSpace(QColorSpace::SRgb));
        break;
    case 1: {
        img.setColorSpace(QColorSpace(QColorSpace::SRgbLinear));
        break;
    }
    }

    // Handle the byte stream
    for (quint32 y = 0; y < qoi.Height; y++) {
        QRgb *scanline = (QRgb *)img.scanLine(y);

        for (quint32 x = 0; x < qoi.Width; x++) {
            if (run > 0) {
                run--;
            } else if (p < chunks_len) {
                quint32 b1 = input[p++];

                if (b1 == QOI_OP_RGB) {
                    px.r = input[p++];
                    px.g = input[p++];
                    px.b = input[p++];
                } else if (b1 == QOI_OP_RGBA) {
                    px.r = input[p++];
                    px.g = input[p++];
                    px.b = input[p++];
                    px.a = input[p++];
                } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                    px = index[b1];
                } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                    px.r += ((b1 >> 4) & 0x03) - 2;
                    px.g += ((b1 >> 2) & 0x03) - 2;
                    px.b += (b1 & 0x03) - 2;
                } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                    quint32 b2 = input[p++];
                    quint32 vg = (b1 & 0x3f) - 32;
                    px.r += vg - 8 + ((b2 >> 4) & 0x0f);
                    px.g += vg;
                    px.b += vg - 8 + (b2 & 0x0f);
                } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                    run = (b1 & 0x3f);
                }
                index[QoiHash(px) % 64] = px;
            }
            // Set the values for the pixel at (x, y)
            scanline[x] = qRgba(px.r, px.g, px.b, px.a);
        }
    }

    return true;
}

} // namespace

QOIHandler::QOIHandler()
{
}

bool QOIHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("qoi");
        return true;
    }
    return false;
}

bool QOIHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("QOIHandler::canRead() called with no device");
        return false;
    }
    if (device->isSequential()) {
        return false;
    }

    qint64 oldPos = device->pos();
    QByteArray head = device->read(QOI_HEADER_SIZE);
    int readBytes = head.size();

    device->seek(oldPos);

    if (readBytes < QOI_HEADER_SIZE) {
        return false;
    }

    QDataStream stream(head);
    stream.setByteOrder(QDataStream::BigEndian);
    QoiHeader qoi;
    stream >> qoi;

    return IsSupported(qoi);
}

bool QOIHandler::read(QImage *image)
{
    QDataStream s(device());
    s.setByteOrder(QDataStream::BigEndian);

    // Read image header
    QoiHeader qoi;
    s >> qoi;

    // Check if file is supported
    if (!IsSupported(qoi)) {
        return false;
    }

    QImage img;
    bool result = LoadQOI(s, qoi, img);

    if (result == false) {
        return false;
    }

    *image = img;
    return true;
}

QImageIOPlugin::Capabilities QOIPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "qoi" || format == "QOI") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && QOIHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *QOIPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new QOIHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}
