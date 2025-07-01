/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2025 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/*
 * Format specifications:
 * - https://wiki.amigaos.net/wiki/IFF_FORM_and_Chunk_Registry
 * - https://www.fileformat.info/format/iff/egff.htm
 */

#ifndef KIMG_CHUNKS_P_H
#define KIMG_CHUNKS_P_H

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QIODevice>
#include <QPoint>
#include <QSize>
#include <QSharedPointer>

// Main chunks (Standard)
#define CAT__CHUNK QByteArray("CAT ")
#define FILL_CHUNK QByteArray("    ")
#define FORM_CHUNK QByteArray("FORM")
#define LIST_CHUNK QByteArray("LIST")
#define PROP_CHUNK QByteArray("PROP")

// Main chuncks (Maya)
#define FOR4_CHUNK QByteArray("FOR4")

// FORM ILBM IFF
#define BMHD_CHUNK QByteArray("BMHD")
#define BODY_CHUNK QByteArray("BODY")
#define CAMG_CHUNK QByteArray("CAMG")
#define CMAP_CHUNK QByteArray("CMAP")
#define DPI__CHUNK QByteArray("DPI ")
#define SHAM_CHUNK QByteArray("SHAM") // undocumented

// FOR4 CIMG IFF (Maya)
#define RGBA_CHUNK QByteArray("RGBA")
#define TBHD_CHUNK QByteArray("TBHD")

// FORx IFF (found on some IFF format specs)
#define AUTH_CHUNK QByteArray("AUTH")
#define DATE_CHUNK QByteArray("DATE")
#define FVER_CHUNK QByteArray("FVER")
#define HIST_CHUNK QByteArray("HIST")
#define VERS_CHUNK QByteArray("VERS")

#define CHUNKID_DEFINE(a) static QByteArray defaultChunkId() { return a; }

/*!
 * \brief The IFFChunk class
 */
class IFFChunk
{
public:
    using ChunkList = QList<QSharedPointer<IFFChunk>>;

    virtual ~IFFChunk();

    /*!
     * \brief IFFChunk
     * Creates invalid chunk.
     * \sa isValid
     */
    IFFChunk();

    IFFChunk(const IFFChunk& other) = default;
    IFFChunk& operator =(const IFFChunk& other) = default;

    bool operator ==(const IFFChunk& other) const;

    /*!
     * \brief isValid
     * \return True if the chunk is valid, otherwise false.
     * \note The default implementation checks that chunkId() contains only valid characters.
     */
    virtual bool isValid() const;

    /*!
     * \brief alignBytes
     * \return The chunk alignment bytes. By default returns bytes set using setAlignBytes().
     */
    virtual qint32 alignBytes() const;

    /*!
     * \brief chunkId
     * \return The chunk Id of this chunk.
     */
    QByteArray chunkId() const;

    /*!
     * \brief bytes
     * \return The size (in bytes) of the chunck data.
     */

    quint32 bytes() const;

    /*!
     * \brief data
     * \return The data stored inside the class. If no data present, use readRawData().
     * \sa readRawData
     */
    const QByteArray& data() const;

    /*!
     * \brief chunks
     * \return The chunks inside this chunk.
     */
    const ChunkList& chunks() const;

    /*!
     * \brief chunkVersion
     * \param cid Chunk Id to extract the version from.
     * \return The version of the chunk. Zero means no valid chunk data.
     */
    static quint8 chunkVersion(const QByteArray& cid);

    /*!
     * \brief isChunkType
     * Check if the chunkId is of type of cid (any version).
     * \param cid Chunk Id to check.
     * \return True on success, otherwise false.
     */
    bool isChunkType(const QByteArray& cid) const;

    /*!
     * \brief readInfo
     * Reads chunkID, size and set the data position.
     * \param d The device.
     * \return True on success, otherwise false.
     */
    bool readInfo(QIODevice *d);

    /*!
     * \brief readStructure
     * Read the internal structure using innerReadStructure() of the Chunk and set device the position to the next chunks.
     * \param d The device.
     * \return True on success, otherwise false.
     */
    bool readStructure(QIODevice *d);

    /*!
     * \brief readRawData
     * \param d The device.
     * \param relPos The position to read relative to the chunk position.
     * \param size The size of the data to read (-1 means all chunk).
     * \return The data read or empty array on error.
     * \note Ignores any data already read and available with data().
     * \sa data
     */
    QByteArray readRawData(QIODevice *d, qint64 relPos = 0, qint64 size = -1) const;

    /*!
     * \brief seek
     * \param d The device.
     * \param relPos The position to read relative to the chunk position.
     * \return True on success, otherwise false.
     */
    bool seek(QIODevice *d, qint64 relPos = 0) const;

    /*!
     * \brief fromDevice
     * \param d The device.
     * \param ok Set to false if errors occurred.
     * \return The chunk list found.
     */
    static ChunkList fromDevice(QIODevice *d, bool *ok = nullptr);

    /*!
     * \brief search
     * Search for a chunk in the list of chunks.
     * \param cid The chunkId to search.
     * \param chunks The list of chunks to search for the requested chunk.
     * \return The list of chunks with the given chunkId.
     */
    static ChunkList search(const QByteArray &cid, const ChunkList& chunks);

    /*!
     * \brief search
     */
    static ChunkList search(const QByteArray &cid, const QSharedPointer<IFFChunk>& chunk);

    /*!
     * \brief searchT
     * Convenient search function to avoid casts.
     * \param chunk The chunk to search for the requested chunk type.
     * \return The list of chunks of T type.
     */
    template <class T>
    static QList<const T*> searchT(const IFFChunk *chunk) {
        QList<const T*> list;
        if (chunk == nullptr)
            return list;
        auto cid = T::defaultChunkId();
        if (chunk->chunkId() == cid)
            if (auto c = dynamic_cast<const T*>(chunk))
                list << c;
        auto tmp = chunk->chunks();
        for (auto &&c : tmp)
            list << searchT<T>(c.data());
        return list;
    }

    /*!
     * \brief searchT
     * Convenient search function to avoid casts.
     * \param chunks The list of chunks to search for the requested chunk.
     * \return The list of chunks of T type.
     */
    template <class T>
    static QList<const T*> searchT(const ChunkList& chunks) {
        QList<const T*> list;
        for (auto &&chunk : chunks)
            list << searchT<T>(chunk.data());
        return list;
    }

    CHUNKID_DEFINE(QByteArray())

protected:
    /*!
     * \brief innerReadStructure
     * Reads data structure. Default implementation does nothing.
     * \param d The device.
     * \return True on success, otherwise false.
     */
    virtual bool innerReadStructure(QIODevice *d);

    /*!
     * \brief setAlignBytes
     * \param bytes
     */
    void setAlignBytes(qint32 bytes)
    {
        _align = bytes;
    }


    /*!
     * \brief cacheData
     * Read all chunk data and store it on _data.
     * \return True on success, otherwise false.
     * \warning This function does not load anything if the chunk size is larger than 8MiB. For larger chunks, use direct data access.
     */
    bool cacheData(QIODevice *d);

    /*!
     * \brief setChunks
     * \param chunks
     */
    void setChunks(const ChunkList &chunks);

    inline quint16 ui16(quint8 c1, quint8 c2) const {
        return (quint16(c2) << 8) | quint16(c1);
    }

    inline qint16 i16(quint8 c1, quint8 c2) const {
        return qint32(ui16(c1, c2));
    }

    inline quint32 ui32(quint8 c1, quint8 c2, quint8 c3, quint8 c4) const {
        return (quint32(c4) << 24) | (quint32(c3) << 16) | (quint32(c2) << 8) | quint32(c1);
    }

    inline qint32 i32(quint8 c1, quint8 c2, quint8 c3, quint8 c4) const {
        return qint32(ui32(c1, c2, c3, c4));
    }

    static ChunkList innerFromDevice(QIODevice *d, bool *ok, qint32 alignBytes);

private:
    char _chunkId[4];

    quint32 _size;

    qint32 _align;

    qint64 _dataPos;

    QByteArray _data;

    ChunkList _chunks;


};

/*!
 * \brief The IffBMHD class
 * Bitmap Header
 */
class BMHDChunk: public IFFChunk
{
public:
    enum Compression {
        Uncompressed = 0,
        Rle = 1
    };

    virtual ~BMHDChunk() override;

    BMHDChunk();
    BMHDChunk(const BMHDChunk& other) = default;
    BMHDChunk& operator =(const BMHDChunk& other) = default;

    virtual bool isValid() const override;

    qint32 width() const;

    qint32 height() const;

    QSize size() const;

    qint32 left() const;

    qint32 top() const;

    quint8 bitplanes() const;

    quint8 masking() const;

    Compression compression() const;

    quint8 padding() const;

    qint16 transparency() const;

    quint8 xAspectRatio() const;

    quint8 yAspectRatio() const;

    quint16 pageWidth() const;

    quint16 pageHeight() const;

    quint32 rowLen() const;

    CHUNKID_DEFINE(BMHD_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The CMAPChunk class
 */
class CMAPChunk : public IFFChunk
{
public:
    virtual ~CMAPChunk() override;
    CMAPChunk();
    CMAPChunk(const CMAPChunk& other) = default;
    CMAPChunk& operator =(const CMAPChunk& other) = default;

    virtual bool isValid() const override;

    QList<QRgb> palette() const;

    CHUNKID_DEFINE(CMAP_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The CAMGChunk class
 */
class CAMGChunk : public IFFChunk
{
public:
    enum ModeId {
        LoResLace = 0x0004,
        HalfBrite = 0x0080,
        LoResDpf = 0x0400,
        Ham = 0x0800,
        HiRes = 0x8000
    };

    Q_DECLARE_FLAGS(ModeIds, ModeId)

    virtual ~CAMGChunk() override;
    CAMGChunk();
    CAMGChunk(const CAMGChunk& other) = default;
    CAMGChunk& operator =(const CAMGChunk& other) = default;

    virtual bool isValid() const override;

    ModeIds modeId() const;

    CHUNKID_DEFINE(CAMG_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The DPIChunk class
 */
class DPIChunk : public IFFChunk
{
public:
    virtual ~DPIChunk() override;
    DPIChunk();
    DPIChunk(const DPIChunk& other) = default;
    DPIChunk& operator =(const DPIChunk& other) = default;

    virtual bool isValid() const override;

    /*!
     * \brief dpiX
     * \return The horizontal resolution in DPI.
     */
    quint16 dpiX() const;

    /*!
     * \brief dpiY
     * \return The vertical resolution in DPI.
     */
    quint16 dpiY() const;

    /*!
     * \brief dotsPerMeterX
     * \return X resolution as wanted by QImage.
     */
    qint32 dotsPerMeterX() const;

    /*!
     * \brief dotsPerMeterY
     * \return Y resolution as wanted by QImage.
     */
    qint32 dotsPerMeterY() const;

    CHUNKID_DEFINE(DPI__CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};


/*!
 * \brief The BODYChunk class
 */
class BODYChunk : public IFFChunk
{
public:
    virtual ~BODYChunk() override;
    BODYChunk();
    BODYChunk(const BODYChunk& other) = default;
    BODYChunk& operator =(const BODYChunk& other) = default;

    virtual bool isValid() const override;

    CHUNKID_DEFINE(BODY_CHUNK)

    /*!
     * \brief readStride
     * \param d The device.
     * \param header The bitmap header.
     * \param camg The CAMG chunk (optional)
     * \param cmap The CMAP chunk (optional)
     * \return The scanline as requested for QImage.
     * \warning Call resetStrideRead() once before this one.
     */
    QByteArray strideRead(QIODevice *d, const BMHDChunk *header, const CAMGChunk *camg = nullptr, const CMAPChunk *cmap = nullptr) const;

    /*!
     * \brief resetStrideRead
     * Reset the stride read set the position at the beginning of the data and reset all buffers.
     * \param d The device
     * \param header The BMHDChunk chunk (mandatory)
     * \param camg The CAMG chunk (optional)
     * \return True on success, otherwise false.
     * \sa strideRead
     */
    bool resetStrideRead(QIODevice *d) const;

private:
    static QByteArray deinterleave(const QByteArray &planes, const BMHDChunk *header, const CAMGChunk *camg = nullptr, const CMAPChunk *cmap = nullptr);

    mutable QByteArray _readBuffer;
};

/*!
 * \brief The FORMChunk class
 */
class FORMChunk : public IFFChunk
{
    QByteArray _type;

public:
    virtual ~FORMChunk() override;
    FORMChunk();
    FORMChunk(const FORMChunk& other) = default;
    FORMChunk& operator =(const FORMChunk& other) = default;

    virtual bool isValid() const override;

    bool isSupported() const;

    QByteArray formType() const;

    QImage::Format format() const;

    QSize size() const;

    CHUNKID_DEFINE(FORM_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};


/*!
 * \brief The FOR4Chunk class
 */
class FOR4Chunk : public IFFChunk
{
    QByteArray _type;

public:
    virtual ~FOR4Chunk() override;
    FOR4Chunk();
    FOR4Chunk(const FOR4Chunk& other) = default;
    FOR4Chunk& operator =(const FOR4Chunk& other) = default;

    virtual bool isValid() const override;

    virtual qint32 alignBytes() const override;

    bool isSupported() const;

    QByteArray formType() const;

    QImage::Format format() const;

    QSize size() const;

    CHUNKID_DEFINE(FOR4_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The TBHDChunk class
 */
class TBHDChunk : public IFFChunk
{
public:
    enum Flag {
        Rgb = 0x01,
        Alpha = 0x02,
        ZBuffer = 0x04,
        Black = 0x10,

        RgbA = Rgb | Alpha
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Compression {
        Uncompressed = 0,
        Rle = 1
    };

    virtual ~TBHDChunk() override;

    TBHDChunk();
    TBHDChunk(const TBHDChunk& other) = default;
    TBHDChunk& operator =(const TBHDChunk& other) = default;

    virtual bool isValid() const override;

    virtual qint32 alignBytes() const override;

    /*!
     * \brief width
     * \return Image width in pixels.
     */
    qint32 width() const;

    /*!
     * \brief height
     * \return Image height in pixels.
     */
    qint32 height() const;

    /*!
     * \brief size
     * \return Image size in pixels.
     */
    QSize size() const;

    /*!
     * \brief left
     * \return
     */
    qint32 left() const;

    /*!
     * \brief top
     * \return
     */
    qint32 top() const;

    /*!
     * \brief flags
     * \return Image flags.
     */
    Flags flags() const;

    /*!
     * \brief bpc
     * \return Byte per channel (1 or 2)
     */
    qint32 bpc() const;

    /*!
     * \brief channels
     * \return
     */
    qint32 channels() const;

    /*!
     * \brief tiles
     * \return The number of tiles of the image.
     */
    quint16 tiles() const;

    /*!
     * \brief compression
     * \return The data compression.
     */
    Compression compression() const;

    /*!
     * \brief format
     * \return
     */
    QImage::Format format() const;

    CHUNKID_DEFINE(TBHD_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The RGBAChunk class
 */
class RGBAChunk : public IFFChunk
{
public:
    virtual ~RGBAChunk() override;
    RGBAChunk();
    RGBAChunk(const RGBAChunk& other) = default;
    RGBAChunk& operator =(const RGBAChunk& other) = default;

    virtual bool isValid() const override;

    virtual qint32 alignBytes() const override;

    /*!
     * \brief isTileCompressed
     * \param header The image header.
     * \return True if the tile is compressed, otherwise false.
     */
    bool isTileCompressed(const TBHDChunk *header) const;

    /*!
     * \brief pos
     * \return The tile position (top-left corner) in the final image.
     */
    QPoint pos() const;

    /*!
     * \brief size
     * \return The tile size in pixels.
     */
    QSize size() const;

    /*!
     * \brief tile
     * Create the tile by reading the data from the device.
     * \param d The device.
     * \param header The image header.
     * \return The image tile.
     */
    QImage tile(QIODevice *d, const TBHDChunk *header) const;

    CHUNKID_DEFINE(RGBA_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;

private:
    QImage compressedTile(QIODevice *d, const TBHDChunk *header) const;

    QImage uncompressedTile(QIODevice *d, const TBHDChunk *header) const;

    QByteArray readStride(QIODevice *d, const TBHDChunk *header) const;

private:
    QPoint _pos;

    QSize _size;

    mutable QByteArray _readBuffer;
};


/*!
 * \brief The AUTHChunk class
 */
class AUTHChunk : public IFFChunk
{
public:
    virtual ~AUTHChunk() override;
    AUTHChunk();
    AUTHChunk(const AUTHChunk& other) = default;
    AUTHChunk& operator =(const AUTHChunk& other) = default;

    virtual bool isValid() const override;

    QString value() const;

    CHUNKID_DEFINE(AUTH_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The DATEChunk class
 */
class DATEChunk : public IFFChunk
{
public:
    virtual ~DATEChunk() override;
    DATEChunk();
    DATEChunk(const DATEChunk& other) = default;
    DATEChunk& operator =(const DATEChunk& other) = default;

    virtual bool isValid() const override;

    QDateTime value() const;

    CHUNKID_DEFINE(DATE_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

/*!
 * \brief The FVERChunk class
 *
 * \warning The specifications on wiki.amigaos.net differ from what I see in a file saved in Maya format. I do not interpret the data for now.
 */
class FVERChunk : public IFFChunk
{
public:
    virtual ~FVERChunk() override;
    FVERChunk();
    FVERChunk(const FVERChunk& other) = default;
    FVERChunk& operator =(const FVERChunk& other) = default;

    virtual bool isValid() const override;

    CHUNKID_DEFINE(FVER_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};


/*!
 * \brief The HISTChunk class
 */
class HISTChunk : public IFFChunk
{
public:
    virtual ~HISTChunk() override;
    HISTChunk();
    HISTChunk(const HISTChunk& other) = default;
    HISTChunk& operator =(const HISTChunk& other) = default;

    virtual bool isValid() const override;

    QString value() const;

    CHUNKID_DEFINE(HIST_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};


/*!
 * \brief The VERSChunk class
 */
class VERSChunk : public IFFChunk
{
public:
    virtual ~VERSChunk() override;
    VERSChunk();
    VERSChunk(const VERSChunk& other) = default;
    VERSChunk& operator =(const VERSChunk& other) = default;

    virtual bool isValid() const override;

    QString value() const;

    CHUNKID_DEFINE(VERS_CHUNK)

protected:
    virtual bool innerReadStructure(QIODevice *d) override;
};

#endif // KIMG_CHUNKS_P_H
