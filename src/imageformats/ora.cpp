/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

    This code is based on Thacher Ulrich PSD loading code released
    on public domain. See: http://tulrich.com/geekstuff/
*/

#include "ora.h"

#include <QImage>
#include <QScopedPointer>

#include <kzip.h>

static constexpr char s_magic[] = "image/openraster";
static constexpr int s_magic_size = sizeof(s_magic) - 1; // -1 to remove the last \0
static constexpr int s_krita3_offset = 0x26;
static constexpr int s_krita4_offset = 0x2B;
static constexpr int s_krita4_64_offset = 0x40;
static constexpr int s_krita5_offset = 0x26;
static constexpr int s_krita5_64_offset = 0x3A;

OraHandler::OraHandler()
{
}

bool OraHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("ora");
        return true;
    }
    return false;
}

bool OraHandler::read(QImage *image)
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

bool OraHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("OraHandler::canRead() called with no device");
        return false;
    }

    if (device->peek(s_krita3_offset + s_magic_size).contains(s_magic)) return true;
    if (device->peek(s_krita4_offset + s_magic_size).contains(s_magic)) return true;
    if (device->peek(s_krita4_64_offset + s_magic_size).contains(s_magic)) return true;
    if (device->peek(s_krita5_offset + s_magic_size).contains(s_magic)) return true;
    if (device->peek(s_krita5_64_offset + s_magic_size).contains(s_magic)) return true;

    return false;
}

QImageIOPlugin::Capabilities OraPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "ora" || format == "ORA") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && OraHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *OraPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new OraHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}
