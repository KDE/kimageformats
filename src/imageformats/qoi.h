/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2023 Ernest Gupik <ernestgupik@wp.pl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIMG_QOI_H
#define KIMG_QOI_H

#include <QImageIOPlugin>

class QOIHandler : public QImageIOHandler
{
public:
    QOIHandler();

    bool canRead() const override;
    bool read(QImage *image) override;

    static bool canRead(QIODevice *device);
};

class QOIPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "qoi.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

#endif // KIMG_QOI_H
