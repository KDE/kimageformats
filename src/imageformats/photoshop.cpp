/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/
#include "photoshop_p.h"
#include "util_p.h"

#include <QLoggingCategory>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(LOG_PSDSHARED, "kf.imageformats.plugins.psdshared", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(LOG_PSDSHARED, "kf.imageformats.plugins.psdshared", QtWarningMsg)
#endif

QString readPascalString(QDataStream &s, qint32 alignBytes, qint32 *size)
{
    qint32 tmp = 0;
    if (size == nullptr)
        size = &tmp;

    quint8 stringSize;
    s >> stringSize;
    *size = sizeof(stringSize);

    QString str;
    if (stringSize > 0) {
        QByteArray ba;
        ba.resize(stringSize);
        auto read = s.readRawData(ba.data(), ba.size());
        if (read > 0) {
            *size += read;
            str = QString::fromLatin1(ba);
        }
    }

    // align
    if (alignBytes > 1)
        if (auto pad = *size % alignBytes)
            *size += s.skipRawData(alignBytes - pad);

    return str;
}

bool writePascalString(const QString &str, QDataStream &s, qint32 alignBytes)
{
    auto data = str.toLatin1();
    if(data.size() > 250) {
        data = data.left(250);
    }
    auto sz = data.size();
    s << quint8(sz);
    if (sz && s.writeRawData(data.data(), sz) != sz) {
        return false;
    }
    for(alignBytes = std::max(1, alignBytes), sz += 1; sz % alignBytes; ++sz) {
        s << char();
    }
    return s.status() == QDataStream::Ok;
}

PSDImageResourceSection readImageResourceSection(QDataStream &s, bool *ok)
{
    PSDImageResourceSection irs;

    bool tmp = true;
    if (ok == nullptr)
        ok = &tmp;
    *ok = true;

    // Section size
    quint32 tmpSize;
    s >> tmpSize;
    qint64 sectioSize = tmpSize;

    // Reading Image resource block
    for (auto size = sectioSize; size > 0;) {

#define DEC_SIZE(value) \
        if ((size -= qint64(value)) < 0) { \
                *ok = false; \
                break; }

        // Length      Description
        // -------------------------------------------------------------------
        // 4           Signature: '8BIM'
        // 2           Unique identifier for the resource. Image resource IDs
        //             contains a list of resource IDs used by Photoshop.
        // Variable    Name: Pascal string, padded to make the size even
        //             (a null name consists of two bytes of 0)
        // 4           Actual size of resource data that follows
        // Variable    The resource data, described in the sections on the
        //             individual resource types. It is padded to make the size
        //             even.

        quint32 signature;
        s >> signature;
        DEC_SIZE(sizeof(signature))
        // NOTE: MeSa signature is not documented but found in some old PSD take from Photoshop 7.0 CD.
        if (signature != S_8BIM && signature != S_MeSa) { // 8BIM and MeSa
            qCDebug(LOG_PSDSHARED) << "Invalid Image Resource Block Signature!";
            *ok = false;
            break;
        }

        // id
        quint16 id;
        s >> id;
        DEC_SIZE(sizeof(id))

        // getting data
        PSDImageResourceBlock irb;

        // name
        qint32 bytes = 0;
        irb.name = readPascalString(s, 2, &bytes);
        DEC_SIZE(bytes)

        // data read
        quint32 dataSize;
        s >> dataSize;
        DEC_SIZE(sizeof(dataSize))
        if (auto dev = s.device()) {
            if (dataSize > size) {
                qCDebug(LOG_PSDSHARED) << "Invalid Image Resource Block Data Size!";
                *ok = false;
                break;
            }
            irb.data = deviceRead(dev, dataSize);
        }
        auto read = irb.data.size();
        if (read > 0) {
            DEC_SIZE(read)
        }
        if (read != qint64(dataSize)) {
            qCDebug(LOG_PSDSHARED) << "Image Resource Block Read Error!";
            *ok = false;
            break;
        }

        if (auto pad = dataSize % 2) {
            auto skipped = s.skipRawData(pad);
            if (skipped > 0) {
                DEC_SIZE(skipped);
            }
        }

        // insert IRB
        irs.insert(ImageResourceId(id), irb);

#undef DEC_SIZE
    }

    return irs;
}

bool writeImageResourceSection(const PSDImageResourceSection &irs, QDataStream &s)
{
    bool ok = false;
    auto ba = irs.toByteArray(&ok);
    if (!ok) {
        return false;
    }
    s << quint32(ba.size());
    if (s.writeRawData(ba.data(), ba.size()) != ba.size()) {
        return false;
    }
    return (s.status() == QDataStream::Ok);
}


QByteArray PSDImageResourceSection::toByteArray(bool *ok) const
{
    QByteArray ba;

    bool tmp = true;
    if (ok == nullptr)
        ok = &tmp;
    *ok = true;

    if (!isEmpty()) {
        QDataStream s(&ba, QDataStream::WriteOnly);
        s.setByteOrder(QDataStream::BigEndian);

        auto ids = keys();
        for(auto &&id : ids) {
            auto irb = value(id);
            if (irb.data.isEmpty()) {
                continue;
            }

            // signature
            s << quint32(S_8BIM);

            // resource id
            s << quint16(id);

            // resource name (2 bytes aligned)
            if (!writePascalString(irb.name, s, 2)) {
                *ok = false;
                return{};
            }

            // data (2 bytes aligned)
            auto sz = irb.data.size();
            s << quint32(sz);
            if (s.writeRawData(irb.data.data(), sz) != sz) {
                *ok = false;
                return{};
            }
            if (sz % 2) {
                s << char();
            }

            if (s.status() != QDataStream::Ok) {
                *ok = false;
                return{};
            }
        }
    }

    Q_ASSERT(ba.size() % 2 == 0);
    return ba;
}

PSDResolutionInfoBlock::PSDResolutionInfoBlock(qint32 ppmX, qint32 ppmY)
    : m_ppmX(ppmX)
    , m_ppmY(ppmY)
{

}

bool PSDResolutionInfoBlock::isValid() const
{
    return m_ppmX > 0 && m_ppmY > 0;
}

PSDResolutionInfoBlock PSDResolutionInfoBlock::fromImage(const QImage &image)
{
    return PSDResolutionInfoBlock(image.dotsPerMeterX(), image.dotsPerMeterY());
}

QByteArray PSDResolutionInfoBlock::toByteArray() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    auto hres = qRoundOrZero(dppm2dpi(m_ppmX) * 65536);
    ds << hres;
    ds << quint16(1); // dpi
    ds << quint16(2); // cm (display)
    auto vres = qRoundOrZero(dppm2dpi(m_ppmY) * 65536);
    ds << vres;
    ds << quint16(1);
    ds << quint16(2);

    if (hres == 0 || vres == 0) {
        return{};
    }

    return ba;
}
