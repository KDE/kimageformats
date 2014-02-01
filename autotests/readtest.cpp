/*
 * Copyright 2014  Alex Merry <alex.merry@kdemail.net>
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

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QTextStream>

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::setApplicationName(QStringLiteral("readtest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Performs basic image conversion checking."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("format"), QStringLiteral("format to test"));

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.count() < 1) {
        QTextStream(stderr) << "Must provide a format\n";
        parser.showHelp(1);
    } else if (args.count() > 1) {
        QTextStream(stderr) << "Too many arguments\n";
        parser.showHelp(1);
    }

    QString suffix = args.at(0);
    QByteArray format = suffix.toLatin1();

    QDir imgdir(QLatin1String(IMAGEDIR "/") + suffix);
    imgdir.setNameFilters(QStringList(QLatin1String("*.") + suffix));
    imgdir.setFilter(QDir::Files);

    int passed = 0;
    int failed = 0;

    QTextStream(stdout) << "********* "
        << "Starting basic read tests for "
        << suffix << " images *********\n";

    foreach (QFileInfo fi, imgdir.entryInfoList()) {
        int suffixPos = fi.filePath().count() - suffix.count();
        QString inputfile = fi.filePath();
        QString expfile = fi.filePath().replace(suffixPos, suffix.count(), "png");
        QString expfilename = QFileInfo(expfile).fileName();

        QImageReader inputReader(inputfile, format.constData());
        QImageReader expReader(expfile, "png");

        QImage inputImage;
        QImage expImage;

        if (!expReader.read(&expImage)) {
            QTextStream(stdout) << "ERROR: " << fi.fileName()
                << ": could not load " << expfilename
                << ": " << expReader.errorString()
                << "\n";
            ++failed;
            continue;
        }
        if (!inputReader.read(&inputImage)) {
            QTextStream(stdout) << "FAIL : " << fi.fileName()
                << ": failed to load: "
                << inputReader.errorString()
                << "\n";
            ++failed;
            continue;
        }
        inputImage = inputImage.convertToFormat(expImage.format());
        if (expImage != inputImage) {
            QTextStream(stdout) << "FAIL : " << fi.fileName()
                << ": differs from " << expfilename << "\n";
            ++failed;
        } else {
            QTextStream(stdout) << "PASS : " << fi.fileName() << "\n";
            ++passed;
        }
    }

    QTextStream(stdout) << "Totals: "
        << passed << " passed, "
        << failed << " failed\n";
    QTextStream(stdout) << "********* "
        << "Finished basic read tests for "
        << suffix << " images *********\n";

    return failed == 0 ? 0 : 1;
}
