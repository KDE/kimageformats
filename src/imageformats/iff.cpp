/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2025 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "chunks_p.h"
#include "iff_p.h"
#include "util_p.h"

#include <QIODevice>
#include <QImage>
#include <QLoggingCategory>
#include <QPainter>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(LOG_IFFPLUGIN, "kf.imageformats.plugins.iff", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(LOG_IFFPLUGIN, "kf.imageformats.plugins.iff", QtWarningMsg)
#endif

class IFFHandlerPrivate
{
public:
    IFFHandlerPrivate() {}
    ~IFFHandlerPrivate() {}

    bool readStructure(QIODevice *d) {
        if (d == nullptr) {
            return {};
        }

        if (!_chunks.isEmpty()) {
            return true;
        }

        auto ok = false;
        auto chunks = IFFChunk::fromDevice(d, &ok);
        if (ok) {
            _chunks = chunks;
        }
        return ok;
    }

    template <class T>
    static QList<const T*> searchForms(const IFFChunk::ChunkList &chunks, bool supportedOnly = true) {
        QList<const T*> list;
        auto cid = T::defaultChunkId();
        auto forms = IFFChunk::search(cid, chunks);
        for (auto &&form : forms) {
            if (auto f = dynamic_cast<const T*>(form.data()))
                if (!supportedOnly || f->isSupported())
                    list << f;
        }
        return list;
    }

    template <class T>
    QList<const T*> searchForms(bool supportedOnly = true) {
        return searchForms<T>(_chunks, supportedOnly);
    }

    IFFChunk::ChunkList _chunks;
};


IFFHandler::IFFHandler()
    : QImageIOHandler()
    , d(new IFFHandlerPrivate)
{
}

bool IFFHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("iff");
        return true;
    }
    return false;
}

bool IFFHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::canRead() called with no device";
        return false;
    }

    if (device->isSequential()) {
        return false;
    }

    // I avoid parsing obviously incorrect files
    auto cid = device->peek(4);
    if (cid != CAT__CHUNK &&
        cid != FORM_CHUNK &&
        cid != LIST_CHUNK &&
        cid != CAT4_CHUNK &&
        cid != FOR4_CHUNK &&
        cid != LIS4_CHUNK) {
        return false;
    }

    auto ok = false;
    auto pos = device->pos();
    auto chunks = IFFChunk::fromDevice(device, &ok);
    if (!device->seek(pos)) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::canRead() unable to reset device position";
    }
    if (ok) {
        auto forms = IFFHandlerPrivate::searchForms<FORMChunk>(chunks, true);
        auto for4s = IFFHandlerPrivate::searchForms<FOR4Chunk>(chunks, true);
        ok = !forms.isEmpty() || !for4s.isEmpty();
    }
    return ok;
}

void addMetadata(QImage& img, const IFFChunk *form)
{
    // standard IFF metadata
    auto annos = IFFChunk::searchT<ANNOChunk>(form);
    if (!annos.isEmpty()) {
        auto anno = annos.first()->value();
        if (!anno.isEmpty()) {
            img.setText(QStringLiteral(META_KEY_DESCRIPTION), anno);
        }
    }
    auto auths = IFFChunk::searchT<AUTHChunk>(form);
    if (!auths.isEmpty()) {
        auto auth = auths.first()->value();
        if (!auth.isEmpty()) {
            img.setText(QStringLiteral(META_KEY_AUTHOR), auth);
        }
    }
    auto dates = IFFChunk::searchT<DATEChunk>(form);
    if (!dates.isEmpty()) {
        auto dt = dates.first()->value();
        if (dt.isValid()) {
            img.setText(QStringLiteral(META_KEY_CREATIONDATE), dt.toString(Qt::ISODate));
        }
    }
    auto copys = IFFChunk::searchT<COPYChunk>(form);
    if (!copys.isEmpty()) {
        auto cp = copys.first()->value();
        if (!cp.isEmpty()) {
            img.setText(QStringLiteral(META_KEY_COPYRIGHT), cp);
        }
    }
    auto names = IFFChunk::searchT<NAMEChunk>(form);
    if (!names.isEmpty()) {
        auto name = names.first()->value();
        if (!name.isEmpty()) {
            img.setText(QStringLiteral(META_KEY_TITLE), name);
        }
    }

    // software info
    auto vers = IFFChunk::searchT<VERSChunk>(form);
    if (!vers.isEmpty()) {
        auto ver = vers.first()->value();
        if (!vers.isEmpty()) {
            img.setText(QStringLiteral(META_KEY_SOFTWARE), ver);
        }
    }

    // SView5 metadata
    auto exifs = IFFChunk::searchT<EXIFChunk>(form);
    if (!exifs.isEmpty()) {
        auto exif = exifs.first()->value();
        exif.updateImageMetadata(img, false);
        exif.updateImageResolution(img);
    }

    auto xmp0s = IFFChunk::searchT<XMP0Chunk>(form);
    if (!xmp0s.isEmpty()) {
        auto xmp = xmp0s.first()->value();
        if (!xmp.isEmpty()) {
            img.setText(QStringLiteral(META_KEY_XMP_ADOBE), xmp);
        }
    }

    auto iccps = IFFChunk::searchT<ICCPChunk>(form);
    if (!iccps.isEmpty()) {
        auto cs = iccps.first()->value();
        if (cs.isValid()) {
            auto iccns = IFFChunk::searchT<ICCNChunk>(form);
            if (!iccns.isEmpty()) {
                auto desc = iccns.first()->value();
                if (!desc.isEmpty())
                    cs.setDescription(desc);
            }
            img.setColorSpace(cs);
        }
    }

    // resolution -> leave after set of EXIF chunk
    auto dpis = IFFChunk::searchT<DPIChunk>(form);
    if (!dpis.isEmpty()) {
        auto &&dpi = dpis.first();
        if (dpi->isValid()) {
            img.setDotsPerMeterX(dpi->dotsPerMeterX());
            img.setDotsPerMeterY(dpi->dotsPerMeterY());
        }
    }
}

bool IFFHandler::readStandardImage(QImage *image)
{
    auto forms = d->searchForms<FORMChunk>();
    if (forms.isEmpty()) {
        return false;
    }
    auto &&form = forms.first();

    // show the first one (I don't have a sample with many images)
    auto headers = IFFChunk::searchT<BMHDChunk>(form);
    if (headers.isEmpty()) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readStandardImage() no supported image found";
        return false;
    }

    // create the image
    auto &&header = headers.first();
    auto img = imageAlloc(header->size(), form->format());
    if (img.isNull()) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readStandardImage() error while allocating the image";
        return false;
    }

    // set color table
    const CAMGChunk *camg = nullptr;
    auto camgs = IFFChunk::searchT<CAMGChunk>(form);
    if (!camgs.isEmpty()) {
        camg = camgs.first();
    }

    const CMAPChunk *cmap = nullptr;
    auto cmaps = IFFChunk::searchT<CMAPChunk>(form);
    if (cmaps.isEmpty()) {
        auto cmyks = IFFChunk::searchT<CMYKChunk>(form);
        for (auto &&cmyk : cmyks)
            cmaps.append(cmyk);
    }
    if (!cmaps.isEmpty()) {
        cmap = cmaps.first();
    }
    if (img.format() == QImage::Format_Indexed8) {
        if (cmap) {
            auto halfbride = BODYChunk::safeModeId(header, camg, cmap) & CAMGChunk::ModeId::HalfBrite ? true : false;
            img.setColorTable(cmap->palette(halfbride));
        }
    }

    // reading image data
    auto bodies = IFFChunk::searchT<BODYChunk>(form);
    if (bodies.isEmpty()) {
        auto abits = IFFChunk::searchT<ABITChunk>(form);
        for (auto &&abit : abits)
            bodies.append(abit);
    }
    if (bodies.isEmpty()) {
        img.fill(0);
    } else {
        auto &&body = bodies.first();
        if (!body->resetStrideRead(device())) {
            qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readStandardImage() error while reading image data";
            return false;
        }
        auto isPbm = form->formType() == PBM__FORM_TYPE;
        for (auto y = 0, h = img.height(); y < h; ++y) {
            auto line = reinterpret_cast<char*>(img.scanLine(y));
            auto ba = body->strideRead(device(), header, camg, cmap, isPbm);
            if (ba.isEmpty()) {
                qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readStandardImage() error while reading image scanline";
                return false;
            }
            memcpy(line, ba.constData(), std::min(img.bytesPerLine(), ba.size()));
        }
    }

    // set metadata (including image resolution)
    addMetadata(img, form);

    *image = img;
    return true;
}

bool IFFHandler::readMayaImage(QImage *image)
{
    auto forms = d->searchForms<FOR4Chunk>();
    if (forms.isEmpty()) {
        return false;
    }
    auto &&form = forms.first();

    // show the first one (I don't have a sample with many images)
    auto headers = IFFChunk::searchT<TBHDChunk>(form);
    if (headers.isEmpty()) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readMayaImage() no supported image found";
        return false;
    }

    // create the image
    auto &&header = headers.first();
    auto img = imageAlloc(header->size(), form->format());
    if (img.isNull()) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readMayaImage() error while allocating the image";
        return false;
    }

    auto &&tiles = IFFChunk::searchT<RGBAChunk>(form);
    if ((tiles.size() & 0xFFFF) != header->tiles()) { // Photoshop, on large images saves more than 65535 tiles
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readMayaImage() tile number mismatch: found" << tiles.size() << "while expected" << header->tiles();
        return false;
    }
    for (auto &&tile : tiles) {
        auto tp = tile->pos();
        auto ts = tile->size();
        if (tp.x() < 0 || tp.x() + ts.width() > img.width()) {
            qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readMayaImage() wrong tile position or size";
            return false;
        }
        if (tp.y() < 0 || tp.y() + ts.height() > img.height()) {
            qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readMayaImage() wrong tile position or size";
            return false;
        }
        // For future releases: it might be a good idea not to use a QPainter
        auto ti = tile->tile(device(), header);
        if (ti.isNull()) {
            qCWarning(LOG_IFFPLUGIN) << "IFFHandler::readMayaImage() error while decoding the tile";
            return false;
        }
        QPainter painter(&img);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(tp, ti);
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    img.mirror(false, true);
#else
    img.flip(Qt::Orientation::Vertical);
#endif
    addMetadata(img, form);

    *image = img;
    return true;
}

bool IFFHandler::read(QImage *image)
{
    if (!d->readStructure(device())) {
        qCWarning(LOG_IFFPLUGIN) << "IFFHandler::read() invalid IFF structure";
        return false;
    }

    if (readStandardImage(image)) {
        return true;
    }

    if (readMayaImage(image)) {
        return true;
    }

    qCWarning(LOG_IFFPLUGIN) << "IFFHandler::read() no supported image found";
    return false;
}

bool IFFHandler::supportsOption(ImageOption option) const
{
    if (option == QImageIOHandler::Size) {
        return true;
    }
    if (option == QImageIOHandler::ImageFormat) {
        return true;
    }
    return false;
}

QVariant IFFHandler::option(ImageOption option) const
{
    QVariant v;

    if (option == QImageIOHandler::Size) {
        if (d->readStructure(device())) {
            auto forms = d->searchForms<FORMChunk>();
            if (!forms.isEmpty())
                if (auto &&form = forms.first())
                    v = QVariant::fromValue(form->size());

            auto for4s = d->searchForms<FOR4Chunk>();
            if (!for4s.isEmpty())
                if (auto &&form = for4s.first())
                    v = QVariant::fromValue(form->size());
        }
    }

    if (option == QImageIOHandler::ImageFormat) {
        if (d->readStructure(device())) {
            auto forms = d->searchForms<FORMChunk>();
            if (!forms.isEmpty())
                if (auto &&form = forms.first())
                    v = QVariant::fromValue(form->format());

            auto for4s = d->searchForms<FOR4Chunk>();
            if (!for4s.isEmpty())
                if (auto &&form = for4s.first())
                    v = QVariant::fromValue(form->format());
        }
    }

    return v;
}

QImageIOPlugin::Capabilities IFFPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "iff" || format == "ilbm" || format == "lbm") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && IFFHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *IFFPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new IFFHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_iff_p.cpp"
