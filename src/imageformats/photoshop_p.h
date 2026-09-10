/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/
#ifndef PHOTOSHOP_P_H
#define PHOTOSHOP_P_H

#include <QDataStream>
#include <QHash>
#include <QImage>
#include <QString>

enum Signature : quint32 {
    S_8BIM = 0x3842494D, // '8BIM'
    S_8B64 = 0x38423634, // '8B64'

    S_MeSa = 0x4D655361   // 'MeSa'
};

enum ColorMode : quint16 {
    CM_BITMAP = 0,
    CM_GRAYSCALE = 1,
    CM_INDEXED = 2,
    CM_RGB = 3,
    CM_CMYK = 4,
    CM_MULTICHANNEL = 7,
    CM_DUOTONE = 8,
    CM_LABCOLOR = 9,
};

enum ImageResourceId : quint16 {
    IRI_RESOLUTIONINFO = 0x03ED,
    IRI_ICCPROFILE = 0x040F,
    IRI_TRANSPARENCYINDEX = 0x0417,
    IRI_ALPHAIDENTIFIERS = 0x041D,
    IRI_VERSIONINFO = 0x0421,
    IRI_EXIFDATA1 = 0x0422,
    IRI_EXIFDATA3 = 0x0423, // never seen
    IRI_XMPMETADATA = 0x0424
};

enum LayerId : quint32 {
    LI_MT16 = 0x4D743136,   // 'Mt16',
    LI_MT32 = 0x4D743332,   // 'Mt32',
    LI_MTRN = 0x4D74726E    // 'Mtrn'
};

/*!
 * \brief The PSDImageResourceBlock class
 * Raw data of a PSD image resource block
 */
struct PSDImageResourceBlock {
    QString name;
    QByteArray data;
};

class PSDImageResourceSection : public QHash<ImageResourceId, PSDImageResourceBlock>
{
public:
    PSDImageResourceSection() : QHash<ImageResourceId, PSDImageResourceBlock>() {}
    PSDImageResourceSection(const PSDImageResourceSection& other) = default;
    PSDImageResourceSection& operator =(const PSDImageResourceSection& other) = default;

    /*!
     * \brief toByteArray
     * \param ok Pointer to the operation result variable.
     * \return The binary IRB to be written into the PSD file.
     */
    QByteArray toByteArray(bool *ok = nullptr) const;
};

class PSDResolutionInfoBlock
{
public:
    PSDResolutionInfoBlock(qint32 ppmX, qint32 ppmY);
    PSDResolutionInfoBlock(const PSDResolutionInfoBlock& other) = default;
    PSDResolutionInfoBlock& operator =(const PSDResolutionInfoBlock& other) = default;

    /*!
     * \brief isValid
     * \return true if both m_ppmX and m_ppmY are grater than 0. Otherwise false.
     */
    bool isValid() const;

    /*!
     * \brief fromImage
     * Initialize the class using the image resolution.
     */
    static PSDResolutionInfoBlock fromImage(const QImage& image);

    /*!
     * \brief toByteArray
     * \return The binary data ready for the IMage Resource Section.
     */
    QByteArray toByteArray() const;

private:
    qint32 m_ppmX;
    qint32 m_ppmY;
};


/*!
 * \brief readPascalString
 * Reads the Pascal string as defined in the PSD specification.
 * \param s The stream.
 * \param alignBytes Alignment of the string.
 * \param size Number of stream bytes used.
 * \return The string read.
 */
QString readPascalString(QDataStream &s, qint32 alignBytes = 1, qint32 *size = nullptr);

/*!
 * \brief writePascalString
 * Writes the Pascal string as defined in the PSD specification.
 * \param str The string to be written.
 * \param s The stream.
 * \param alignBytes Alignment of the string.
 * \return True on success, otherwise false.
 */
bool writePascalString(const QString& str, QDataStream &s, qint32 alignBytes = 1);

/*!
 * \brief readImageResourceSection
 * Reads the image resource section.
 * \param s The stream.
 * \param ok Pointer to the operation result variable.
 * \return The image resource section raw data.
 */
PSDImageResourceSection readImageResourceSection(QDataStream &s, bool *ok = nullptr);

/*!
 * \brief writeImageResourceSection
 * Writes the image resource section.
 * \param irs The image resource section raw data.
 * \param s The stream.
 * \return True on success, otherwise false.
 */
bool writeImageResourceSection(const PSDImageResourceSection& irs, QDataStream &s);

#endif // PHOTOSHOP_P_H
