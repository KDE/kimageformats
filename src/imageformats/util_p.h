/*
    SPDX-FileCopyrightText: 2022 Albert Astals Cid <aacid@kde.org>
    SPDX-FileCopyrightText: 2022 Mirco Miranda <mircomir@outlook.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef UTIL_P_H
#define UTIL_P_H

#include <limits>

#include <QImage>
#include <QImageIOHandler>

// Image metadata keys to use in plugins (so they are consistent)
#define META_KEY_DESCRIPTION "Description"
#define META_KEY_MANUFACTURER "Manufacturer"
#define META_KEY_SOFTWARE "Software"
#define META_KEY_MODEL "Model"
#define META_KEY_AUTHOR "Author"
#define META_KEY_COPYRIGHT "Copyright"
#define META_KEY_CREATIONDATE "CreationDate"
#define META_KEY_TITLE "Title"
#define META_KEY_DOCUMENTNAME "DocumentName"
#define META_KEY_HOSTCOMPUTER "HostComputer"
#define META_KEY_XMP "XML:com.adobe.xmp"

// QList uses some extra space for stuff, hence the 32 here suggested by Thiago Macieira
static constexpr int kMaxQVectorSize = std::numeric_limits<int>::max() - 32;

// On Qt 6 to make the plugins fail to allocate if the image size is greater than QImageReader::allocationLimit()
// it is necessary to allocate the image with QImageIOHandler::allocateImage().
inline QImage imageAlloc(const QSize &size, const QImage::Format &format)
{
    QImage img;
    if (!QImageIOHandler::allocateImage(size, format, &img)) {
        img = QImage(); // paranoia
    }
    return img;
}

inline QImage imageAlloc(qint32 width, qint32 height, const QImage::Format &format)
{
    return imageAlloc(QSize(width, height), format);
}

#endif // UTIL_P_H
