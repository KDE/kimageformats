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
