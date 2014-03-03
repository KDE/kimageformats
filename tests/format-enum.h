/*
 * Copyright 2014 Alex Merry <alex.merry@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

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

