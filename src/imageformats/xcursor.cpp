/*
 * SPDX-FileCopyrightText: 2026 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "xcursor_p.h"

#include <QImage>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QVariant>

#include <algorithm>

#include "util_p.h"

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(LOG_XCURSORPLUGIN, "kf.imageformats.plugins.xcursor", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(LOG_XCURSORPLUGIN, "kf.imageformats.plugins.xcursor", QtWarningMsg)
#endif

using namespace Qt::StringLiterals;

static constexpr quint32 XCURSOR_MAGIC = 0x72756358; // "Xcur"
static constexpr quint32 XCURSOR_IMAGE_TYPE = 0xfffd0002;

XCursorHandler::XCursorHandler() = default;

bool XCursorHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("xcursor");
        return true;
    }

    // Check if there's another frame coming.
    QDataStream stream(device());
    stream.setByteOrder(QDataStream::LittleEndian);

    // no peek on QDataStream...
    const auto oldPos = device()->pos();
    auto cleanup = qScopeGuard([this, oldPos] {
        device()->seek(oldPos);
    });

    quint32 headerSize, type, subtype, version, width, height, xhot, yhot, delay;
    stream >> headerSize >> type >> subtype >> version >> width >> height >> xhot >> yhot >> delay;

    if (type != XCURSOR_IMAGE_TYPE || width == 0 || height == 0) {
        return false;
    }

    return true;
}

bool XCursorHandler::read(QImage *outImage)
{
    if (!ensureScanned()) {
        return false;
    }

    const auto firstFrameOffset = m_images.value(m_currentSize).first();
    if (device()->pos() < firstFrameOffset) {
        device()->seek(firstFrameOffset);
    }

    QDataStream stream(device());
    stream.setByteOrder(QDataStream::LittleEndian);

    quint32 headerSize, type, subtype, version, width, height, xhot, yhot, delay;
    stream >> headerSize >> type >> subtype >> version >> width >> height >> xhot >> yhot >> delay;

    if (type != XCURSOR_IMAGE_TYPE || width == 0 || height == 0) {
        return false;
    }

    QImage image = imageAlloc(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        return false;
    }

    const qsizetype byteCount = width * height * sizeof(quint32);
    if (stream.readRawData(reinterpret_cast<char *>(image.bits()), byteCount) != byteCount) {
        return false;
    }

    *outImage = image;
    ++m_currentImageNumber;
    m_nextImageDelay = delay;
    m_hotspot = QPoint(xhot, yhot);

    return !image.isNull();
}

int XCursorHandler::currentImageNumber() const
{
    if (!ensureScanned()) {
        return 0;
    }
    return m_currentImageNumber;
}

int XCursorHandler::imageCount() const
{
    if (!ensureScanned()) {
        return 0;
    }
    return m_images.value(m_currentSize).count();
}

bool XCursorHandler::jumpToImage(int imageNumber)
{
    if (!ensureScanned()) {
        return false;
    }

    if (imageNumber < 0) {
        return false;
    }

    if (imageNumber == m_currentImageNumber) {
        return true;
    }

    if (imageNumber >= imageCount()) {
        return false;
    }

    if (!device()->seek(m_images.value(m_currentSize).at(imageNumber))) {
        return false;
    }

    return true;
}

bool XCursorHandler::jumpToNextImage()
{
    if (!ensureScanned()) {
        return false;
    }
    return jumpToImage(m_currentImageNumber + 1);
}

int XCursorHandler::loopCount() const
{
    if (!ensureScanned()) {
        return 0;
    }
    return -1;
}

int XCursorHandler::nextImageDelay() const
{
    if (!ensureScanned()) {
        return 0;
    }
    return m_nextImageDelay;
}

bool XCursorHandler::supportsOption(ImageOption option) const
{
    return option == Size || option == ScaledSize || option == Description || option == Animation;
}

QVariant XCursorHandler::option(ImageOption option) const
{
    if (!supportsOption(option) || !ensureScanned()) {
        return QVariant();
    }

    switch (option) {
    case QImageIOHandler::Size:
        return QSize(m_currentSize, m_currentSize);
    case QImageIOHandler::Description: {
        QString description;

        if (m_hotspot.has_value()) {
            description.append(u"HotspotX: %1\n\n"_s.arg(m_hotspot->x()));
            description.append(u"HotspotY: %1\n\n"_s.arg(m_hotspot->y()));
        }

        // TODO std::transform...
        QStringList stringSizes;
        stringSizes.reserve(m_images.size());
        for (auto it = m_images.keyBegin(); it != m_images.keyEnd(); ++it) {
            stringSizes.append(QString::number(*it));
        }
        description.append(u"Sizes: %1\n\n"_s.arg(stringSizes.join(','_L1)));

        return description;
    }

    case QImageIOHandler::Animation:
        return imageCount() > 1;
    default:
        break;
    }

    return QVariant();
}

void XCursorHandler::setOption(ImageOption option, const QVariant &value)
{
    switch (option) {
    case QImageIOHandler::ScaledSize:
        m_scaledSize = value.toSize();
        pickSize();
        break;
    default:
        break;
    }
}

bool XCursorHandler::ensureScanned() const
{
    if (m_scanned) {
        return true;
    }

    if (device()->isSequential()) {
        return false;
    }

    auto *mutableThis = const_cast<XCursorHandler *>(this);

    const auto oldPos = device()->pos();
    auto cleanup = qScopeGuard([this, oldPos] {
        device()->seek(oldPos);
    });

    device()->seek(0);

    const QByteArray intro = device()->read(4);
    if (intro != "Xcur") {
        return false;
    }

    QDataStream stream(device());
    stream.setByteOrder(QDataStream::LittleEndian);

    quint32 headerSize, version, ntoc;
    stream >> headerSize >> version >> ntoc;

    // TODO headerSize
    // TODO version

    if (!ntoc) {
        return false;
    }

    mutableThis->m_images.clear();

    for (quint32 i = 0; i < ntoc; ++i) {
        quint32 type, size, position;
        stream >> type >> size >> position;

        if (type != XCURSOR_IMAGE_TYPE) {
            continue;
        }

        mutableThis->m_images[size].append(position);
    }

    mutableThis->pickSize();

    return !m_images.isEmpty();
}

void XCursorHandler::pickSize()
{
    if (m_images.isEmpty()) {
        return;
    }

    // If a scaled size was requested, find the closest match.
    const auto sizes = m_images.keys();
    // If no scaled size is specified, use the biggest one available.
    m_currentSize = sizes.last();

    if (!m_scaledSize.isEmpty()) {
        // TODO Use some clever algo iterator thing instead of keys()...
        const int wantedSize = std::max(m_scaledSize.width(), m_scaledSize.height());
        // Prefer downsampling over upsampling.
        for (int i = sizes.size() - 1; i >= 0; --i) {
            const int size = sizes.at(i);
            if (size < wantedSize) {
                break;
            }

            m_currentSize = size;
        }
    }
}

bool XCursorHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_XCURSORPLUGIN) << "XCurosorHandler::canRead() called with no device";
        return false;
    }
    if (device->isSequential()) {
        return false;
    }

    const QByteArray intro = device->peek(4 * 4);

    if (intro.length() != 4 * 4) {
        return false;
    }

    if (!intro.startsWith("Xcur")) {
        return false;
    }

    // TODO sanity check sizes?

    return true;
}

QImageIOPlugin::Capabilities XCursorPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "xcursor") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && XCursorHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *XCursorPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new XCursorHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_xcursor_p.cpp"
