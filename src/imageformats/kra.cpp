/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

    This code is based on Thacher Ulrich PSD loading code released
    on public domain. See: http://tulrich.com/geekstuff/
*/

#include "kra.h"

#include <kzip.h>

#include <QFile>
#include <QIODevice>
#include <QImage>

static constexpr char s_magic[] = "application/x-krita";
static constexpr int s_magic_size = sizeof(s_magic) - 1; // -1 to remove the last \0
static constexpr int s_krita3_offset = 0x26;
static constexpr int s_krita4_offset = 0x2B;
static constexpr int s_krita4_64_offset = 0x40;
static constexpr int s_krita5_offset = 0x26;
static constexpr int s_krita5_64_offset = 0x3A;

KraHandler::KraHandler()
{
}

bool KraHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("kra");
        return true;
    }
    return false;
}

bool KraHandler::read(QImage *image)
{
    KZip zip(device());
    if (!zip.open(QIODevice::ReadOnly)) {
        return false;
    }

    const KArchiveEntry *entry = zip.directory()->entry(QStringLiteral("mergedimage.png"));
    if (!entry || !entry->isFile()) {
        return false;
    }

    const KZipFileEntry *fileZipEntry = static_cast<const KZipFileEntry *>(entry);

    image->loadFromData(fileZipEntry->data(), "PNG");

    return true;
}

bool KraHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("KraHandler::canRead() called with no device");
        return false;
    }

    ByteArray ba = device->peek(43 + s_magic_size);
    return (ba.contains(s_magic));

    return false;
}

QImageIOPlugin::Capabilities KraPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "kra" || format == "KRA") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && KraHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *KraPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new KraHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}
