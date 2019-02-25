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

#include "format-enum.h"

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
        const QList<QByteArray> lstSupportedFormats = QImageReader::supportedImageFormats();
        for (const QByteArray &fmt : lstSupportedFormats) {
            out << "  " << fmt << '\n';
        }
        return 0;
    }
    if (parser.isSet(listqformats)) {
        QTextStream out(stdout);
        out << "QImage formats:\n";
        // skip QImage::Format_Invalid
        for (int i = 1; i < qimage_format_enum_names_count; ++i) {
            out << "  " << qimage_format_enum_names[i] << '\n';
        }
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
