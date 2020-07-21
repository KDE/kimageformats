/*
    SPDX-FileCopyrightText: 2014 Alex Merry <alex.merry@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef FORMAT_ENUM_H
#define FORMAT_ENUM_H

#include <QImage>

// Generated from QImage::Format enum
static const char * qimage_format_enum_names[] = {
    "Invalid",
    "Mono",
    "MonoLSB",
    "Indexed8",
    "RGB32",
    "ARGB32",
    "ARGB32_Premultiplied",
    "RGB16",
    "ARGB8565_Premultiplied",
    "RGB666",
    "ARGB6666_Premultiplied",
    "RGB555",
    "ARGB8555_Premultiplied",
    "RGB888",
    "RGB444",
    "ARGB4444_Premultiplied",
    "RGBX8888",
    "RGBA8888",
    "RGBA8888_Premultiplied"
};
// Never claim there are more than QImage::NImageFormats supported formats.
// This is future-proofing against the above list being extended.
static const int qimage_format_enum_names_count =
    (sizeof(qimage_format_enum_names) / sizeof(*qimage_format_enum_names) > int(QImage::NImageFormats))
    ? int(QImage::NImageFormats)
    : (sizeof(qimage_format_enum_names) / sizeof(*qimage_format_enum_names));

QImage::Format formatFromString(const QString &str)
{
    for (int i = 0; i < qimage_format_enum_names_count; ++i) {
        if (str.compare(QLatin1String(qimage_format_enum_names[i]), Qt::CaseInsensitive) == 0) {
            return (QImage::Format)(i);
        }
    }
    return QImage::Format_Invalid;
}

QString formatToString(QImage::Format format)
{
    int index = int(format);
    if (index > 0 && index < qimage_format_enum_names_count) {
        return QLatin1String(qimage_format_enum_names[index]);
    }
    return QLatin1String("<unknown:") +
        QString::number(index) +
        QLatin1String(">");
}

#endif
