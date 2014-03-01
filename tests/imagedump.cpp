/*
 * Copyright 2013  Alex Merry <alex.merry@kdemail.net>
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

#include <stdio.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QImageReader>
#include <QFile>
#include <QMetaObject>
#include <QMetaEnum>
#include <QTextStream>

QImage::Format formatFromString(const QString &str)
{
    if (str.compare(QLatin1String("Mono"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_Mono;
    } else if (str.compare(QLatin1String("MonoLSB"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_MonoLSB;
    } else if (str.compare(QLatin1String("Indexed8"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_Indexed8;
    } else if (str.compare(QLatin1String("RGB32"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGB32;
    } else if (str.compare(QLatin1String("ARGB32"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_ARGB32;
    } else if (str.compare(QLatin1String("ARGB32_Premultiplied"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_ARGB32_Premultiplied;
    } else if (str.compare(QLatin1String("RGB16"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGB16;
    } else if (str.compare(QLatin1String("ARGB8565_Premultiplied"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_ARGB8565_Premultiplied;
    } else if (str.compare(QLatin1String("RGB666"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGB666;
    } else if (str.compare(QLatin1String("ARGB6666_Premultiplied"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_ARGB6666_Premultiplied;
    } else if (str.compare(QLatin1String("RGB555"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGB555;
    } else if (str.compare(QLatin1String("ARGB8555_Premultiplied"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_ARGB8555_Premultiplied;
    } else if (str.compare(QLatin1String("RGB888"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGB888;
    } else if (str.compare(QLatin1String("RGB444"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGB444;
    } else if (str.compare(QLatin1String("ARGB4444_Premultiplied"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_ARGB4444_Premultiplied;
    } else if (str.compare(QLatin1String("RGBX8888"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGBX8888;
    } else if (str.compare(QLatin1String("RGBA8888"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGBA8888;
    } else if (str.compare(QLatin1String("RGBA8888_Premultiplied"), Qt::CaseInsensitive) == 0) {
        return QImage::Format_RGBA8888_Premultiplied;
    } else {
        return QImage::Format_Invalid;
    }
}

QString formatToString(QImage::Format format)
{
    switch (format) {
        case QImage::Format_Invalid:
            return QStringLiteral("Invalid");
        case QImage::Format_Mono:
            return QStringLiteral("Mono");
        case QImage::Format_MonoLSB:
            return QStringLiteral("MonoLSB");
        case QImage::Format_Indexed8:
            return QStringLiteral("Indexed8");
        case QImage::Format_RGB32:
            return QStringLiteral("RGB32");
        case QImage::Format_ARGB32:
            return QStringLiteral("ARGB32");
        case QImage::Format_ARGB32_Premultiplied:
            return QStringLiteral("ARGB32_Premultiplied");
        case QImage::Format_RGB16:
            return QStringLiteral("RGB16");
        case QImage::Format_ARGB8565_Premultiplied:
            return QStringLiteral("ARGB8565_Premultiplied");
        case QImage::Format_RGB666:
            return QStringLiteral("RGB666");
        case QImage::Format_ARGB6666_Premultiplied:
            return QStringLiteral("ARGB6666_Premultiplied");
        case QImage::Format_RGB555:
            return QStringLiteral("RGB555");
        case QImage::Format_ARGB8555_Premultiplied:
            return QStringLiteral("ARGB8555_Premultiplied");
        case QImage::Format_RGB888:
            return QStringLiteral("RGB888");
        case QImage::Format_RGB444:
            return QStringLiteral("RGB444");
        case QImage::Format_ARGB4444_Premultiplied:
            return QStringLiteral("ARGB4444_Premultiplied");
        case QImage::Format_RGBX8888:
            return QStringLiteral("RGBX8888");
        case QImage::Format_RGBA8888:
            return QStringLiteral("RGBA8888");
        case QImage::Format_RGBA8888_Premultiplied:
            return QStringLiteral("RGBA8888_Premultiplied");
        default:
            return QLatin1String("<unknown:") +
                QString::number(int(format)) +
                QLatin1String(">");
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::setApplicationName(QStringLiteral("imagedump"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Dumps the content of QImage::bits()"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("image"), QStringLiteral("image file"));
    parser.addPositionalArgument(QStringLiteral("datafile"), QStringLiteral("file QImage data should be written to"));
    QCommandLineOption informat(
        QStringList() << QStringLiteral("f") << QStringLiteral("file-format"),
        QStringLiteral("Image file format"),
        QStringLiteral("format"));
    parser.addOption(informat);
    QCommandLineOption qimgformat(
        QStringList() << QStringLiteral("q") << QStringLiteral("qimage-format"),
        QStringLiteral("QImage data format"),
        QStringLiteral("format"));
    parser.addOption(qimgformat);
    QCommandLineOption listformats(
        QStringList() << QStringLiteral("l") << QStringLiteral("list-file-formats"),
        QStringLiteral("List supported image file formats"));
    parser.addOption(listformats);
    QCommandLineOption listqformats(
        QStringList() << QStringLiteral("p") << QStringLiteral("list-qimage-formats"),
        QStringLiteral("List supported QImage data formats"));
    parser.addOption(listqformats);

    parser.process(app);

    const QStringList files = parser.positionalArguments();

    if (parser.isSet(listformats)) {
        QTextStream out(stdout);
        out << "File formats:\n";
        foreach (const QByteArray &fmt, QImageReader::supportedImageFormats()) {
            out << "  " << fmt << '\n';
        }
        return 0;
    }
    if (parser.isSet(listqformats)) {
        QTextStream(stdout)
            << "QImage formats:\n"
            << "  Mono\n"
            << "  MonoLSB\n"
            << "  Indexed8\n"
            << "  RGB32\n"
            << "  ARGB32\n"
            << "  ARGB32_Premultiplied\n"
            << "  RGB16\n"
            << "  ARGB8565_Premultiplied\n"
            << "  RGB666\n"
            << "  ARGB6666_Premultiplied\n"
            << "  RGB555\n"
            << "  ARGB8555_Premultiplied\n"
            << "  RGB888\n"
            << "  RGB444\n"
            << "  ARGB4444_Premultiplied\n"
            << "  RGBX8888\n"
            << "  RGBA8888\n"
            << "  RGBA8888_Premultiplied\n";
        return 0;
    }

    if (files.count() != 2) {
        QTextStream(stderr) << "Must provide exactly two files\n";
        parser.showHelp(1);
    }
    QImageReader reader(files.at(0), parser.value(informat).toLatin1());
    QImage img = reader.read();
    if (img.isNull()) {
        QTextStream(stderr) << "Could not read image: "
                            << reader.errorString() << '\n';
        return 2;
    }

    QFile output(files.at(1));
    if (!output.open(QIODevice::WriteOnly)) {
        QTextStream(stderr) << "Could not open " << files.at(1)
                            << " for writing: "
                            << output.errorString() << '\n';
        return 3;
    }
    if (parser.isSet(qimgformat)) {
        QImage::Format qformat = formatFromString(parser.value(qimgformat));
        if (qformat == QImage::Format_Invalid) {
            QTextStream(stderr) << "Unknown QImage data format "
                                << parser.value(qimgformat) << '\n';
            return 4;
        }
        img = img.convertToFormat(qformat);
    }
    qint64 written = output.write(reinterpret_cast<const char *>(img.bits()), img.byteCount());
    if (written != img.byteCount()) {
        QTextStream(stderr) << "Could not write image data to " << files.at(1)
                            << ":" << output.errorString() << "\n";
        return 5;
    }
    QTextStream(stdout) << "Created " << files.at(1) << " with data format "
                        << formatToString(img.format()) << "\n";

    return 0;
}
