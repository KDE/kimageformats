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
#include <QImageWriter>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::setApplicationName(QStringLiteral("imageconverter"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.01.01.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Converts images from one format to another"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("in"), QStringLiteral("input image file"));
    parser.addPositionalArgument(QStringLiteral("out"), QStringLiteral("output image file"));
    QCommandLineOption informat(
        QStringList() << QStringLiteral("i") << QStringLiteral("informat"),
        QStringLiteral("Image format for input file"),
        QStringLiteral("format"));
    parser.addOption(informat);
    QCommandLineOption outformat(
        QStringList() << QStringLiteral("o") << QStringLiteral("outformat"),
        QStringLiteral("Image format for output file"),
        QStringLiteral("format"));
    parser.addOption(outformat);
    QCommandLineOption listformats(
        QStringList() << QStringLiteral("l") << QStringLiteral("list"),
        QStringLiteral("List supported image formats"));
    parser.addOption(listformats);

    parser.process(app);

    const QStringList files = parser.positionalArguments();

    if (parser.isSet(listformats)) {
        QTextStream out(stdout);
        out << "Input formats:\n";
        foreach (const QByteArray &fmt, QImageReader::supportedImageFormats()) {
            out << "  " << fmt << '\n';
        }
        out << "Output formats:\n";
        foreach (const QByteArray &fmt, QImageWriter::supportedImageFormats()) {
            out << "  " << fmt << '\n';
        }
        return 0;
    }

    if (files.count() != 2) {
        QTextStream(stdout) << "Must provide exactly two files\n";
        parser.showHelp(1);
    }
    QImageReader reader(files.at(0), parser.value(informat).toLatin1());
    QImage img = reader.read();
    if (img.isNull()) {
        QTextStream(stdout) << "Could not read image: " << reader.errorString() << '\n';
        return 2;
    }

    QImageWriter writer(files.at(1), parser.value(outformat).toLatin1());
    if (!writer.write(img)) {
        QTextStream(stdout) << "Could not write image: " << writer.errorString() << '\n';
        return 3;
    }

    return 0;
}
