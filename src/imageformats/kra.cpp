/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Boudewijn Rempt <boud@valdyas.org>
    SPDX-FileCopyrightText: 2026 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later

    This code is based on Thacher Ulrich PSD loading code released
    on public domain. See: http://tulrich.com/geekstuff/
*/

#include "kra_p.h"
#include "util_p.h"

#include <kzip.h>

#include <QByteArrayView>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QLoggingCategory>
#include <QXmlStreamReader>

Q_DECLARE_LOGGING_CATEGORY(LOG_KRAPLUGIN)
Q_LOGGING_CATEGORY(LOG_KRAPLUGIN, "kf.imageformats.plugins.kra", QtWarningMsg)

#define ORA_MAGIC QByteArrayView("image/openraster")
#define KRA_MAGIC QByteArrayView("application/x-krita")

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

static bool addMetadata(QImage *image, const QByteArray& rawXml)
{
    if (image == nullptr) {
        return false;
    }
    QXmlStreamReader xml(rawXml);
    for(QString key; !xml.atEnd();) {
        auto tt = xml.readNext();
        if (tt == QXmlStreamReader::StartElement) {
            key = xml.name().toString().toLower();
        }
        else if (tt == QXmlStreamReader::EndElement) {
            key.clear();
        }
        else if (tt == QXmlStreamReader::Characters) {
            auto text = xml.text().toString().trimmed();
            if (text.isEmpty() || key.isEmpty())
                continue;
            if (key == QStringLiteral("title")) {
                image->setText(QStringLiteral(META_KEY_TITLE), text);
            }
            else if (key == QStringLiteral("abstract")) {
                image->setText(QStringLiteral(META_KEY_DESCRIPTION), text);
            }
            else if (key == QStringLiteral("full-name")) {
                image->setText(QStringLiteral(META_KEY_AUTHOR), text);
            }
            else if (key == QStringLiteral("date")) {
                if (QDateTime::fromString(text, Qt::ISODate).isValid())
                    image->setText(QStringLiteral(META_KEY_MODIFICATIONDATE), text);
            }
            else if (key == QStringLiteral("creation-date")) {
                if (QDateTime::fromString(text, Qt::ISODate).isValid())
                    image->setText(QStringLiteral(META_KEY_CREATIONDATE), text);
            }
            else if (key == QStringLiteral("keyword")) {
                image->setText(QStringLiteral(META_KEY_KEYWORDS), text);
            }
            else if (key == QStringLiteral("license")) {
                image->setText(QStringLiteral(META_KEY_COPYRIGHT), text);
            }
            else {
                qCDebug(LOG_KRAPLUGIN) << "Unmanaged metadata:" << key << text;
            }
        }
    }
    return !xml.hasError();
}

bool KraHandler::read(QImage *image)
{
    KZip zip(device());
    if (!zip.open(QIODevice::ReadOnly)) {
        return false;
    }

    // reading the image
    const KArchiveEntry *entry = zip.directory()->entry(QStringLiteral("mergedimage.png"));
    if (!entry || !entry->isFile()) {
        return false;
    }
    const KZipFileEntry *fileZipEntry = static_cast<const KZipFileEntry *>(entry);
    if (!image->loadFromData(fileZipEntry->data(), "PNG")) {
        qCCritical(LOG_KRAPLUGIN) << "Invalid image.";
        return false;
    }

    // reading metadata
    const KArchiveEntry *metaEntry = zip.directory()->entry(QStringLiteral("documentinfo.xml"));
    if (!metaEntry || !metaEntry->isFile()) {
        return true; // the image is still valid
    }
    const KZipFileEntry *metaZipEntry = static_cast<const KZipFileEntry *>(metaEntry);
    if (!addMetadata(image, metaZipEntry->data())) {
        qCWarning(LOG_KRAPLUGIN) << "XML metadat seems invalid.";
    }

    return true;
}

bool KraHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_KRAPLUGIN) << "KraHandler::canRead() called with no device";
        return false;
    }
    if (device->isSequential()) {
        return false;
    }

    auto head = device->peek(100);
    if (!head.startsWith(QByteArrayView("PK"))) {
        return false;
    }
    return head.contains(KRA_MAGIC) || head.contains(ORA_MAGIC);
}

QImageIOPlugin::Capabilities KraPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "kra" || format == "ora") {
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

#include "moc_kra_p.cpp"
