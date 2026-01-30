/*
 * SPDX-FileCopyrightText: 2026 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIMG_XCURSOR_P_H
#define KIMG_XCURSOR_P_H

#include <QImageIOPlugin>
#include <QSize>

#include <optional>

struct XCursorImage {
    qint64 offset;
    quint32 delay;
};

class XCursorHandler : public QImageIOHandler
{
public:
    XCursorHandler();

    bool canRead() const override;
    bool read(QImage *image) override;

    int currentImageNumber() const override;
    int imageCount() const override;
    bool jumpToImage(int imageNumber) override;
    bool jumpToNextImage() override;

    int loopCount() const override;
    int nextImageDelay() const override;

    bool supportsOption(ImageOption option) const override;
    QVariant option(ImageOption option) const override;
    void setOption(ImageOption option, const QVariant &value) override;

    static bool canRead(QIODevice *device);

private:
    bool ensureScanned() const;
    void pickSize();

    bool m_scanned = false;

    int m_currentImageNumber = 0;

    QSize m_scaledSize;
    int m_currentSize = 0;

    QMap<int /*size*/, QVector<qint64 /*offset*/>> m_images;

    int m_nextImageDelay = 0;
    std::optional<QPoint> m_hotspot;
};

class XCursorPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "xcursor.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

#endif // KIMG_XCURSOR_P_H
