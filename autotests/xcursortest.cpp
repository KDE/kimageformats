/*
 * SPDX-FileCopyrightText: 2026 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#include <QImage>
#include <QImageReader>
#include <QTest>

using namespace Qt::StringLiterals;

static bool imgEquals(const QImage &im1, const QImage &im2)
{
    const int height = im1.height();
    const int width = im1.width();
    for (int i = 0; i < height; ++i) {
        const auto *line1 = reinterpret_cast<const quint8 *>(im1.scanLine(i));
        const auto *line2 = reinterpret_cast<const quint8 *>(im2.scanLine(i));
        for (int j = 0; j < width; ++j) {
            if (line1[j] - line2[j] != 0) {
                return false;
            }
        }
    }
    return true;
}

class XCursorTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
    }

    void testReadMetadata()
    {
        QImageReader reader(QFINDTESTDATA("xcursor/wait"));

        QVERIFY(reader.canRead());

        QCOMPARE(reader.imageCount(), 18);

        // By default it chooses the largest size
        QCOMPARE(reader.size(), QSize(72, 72));

        QCOMPARE(reader.text(u"Sizes"_s), u"24,48,72"_s);
    }

    void testRead_data()
    {
        QTest::addColumn<int>("size");
        QTest::addColumn<int>("reference");

        // It prefers downsampling over upsampling.
        QTest::newRow("12px") << 12 << 24;
        QTest::newRow("24px") << 24 << 24;
        QTest::newRow("48px") << 48 << 48;
        QTest::newRow("50px") << 50 << 72;
        QTest::newRow("72px") << 72 << 72;
        QTest::newRow("default") << 0 << 72;
    }

    void testRead()
    {
        QFETCH(int, size);
        QFETCH(int, reference);

        QImageReader reader(QFINDTESTDATA("xcursor/wait"));
        QVERIFY(reader.canRead());
        QCOMPARE(reader.currentImageNumber(), 0);

        if (size) {
            reader.setScaledSize(QSize(size, size));
        }

        QCOMPARE(reader.size(), QSize(reference, reference));

        QImage aniFrame;
        QVERIFY(reader.read(&aniFrame));

        QImage img1(QFINDTESTDATA(u"xcursor/wait_%1_1.png"_s.arg(reference)));
        img1.convertTo(aniFrame.format());

        QVERIFY(imgEquals(aniFrame, img1));

        QCOMPARE(reader.nextImageDelay(), 40);
        QCOMPARE(reader.text(u"HotspotX"_s), u"48"_s);
        QCOMPARE(reader.text(u"HotspotY"_s), u"48"_s);

        QVERIFY(reader.canRead());
        // that read() above should have advanced us to the next frame
        QCOMPARE(reader.currentImageNumber(), 1);

        QVERIFY(reader.read(&aniFrame));
        QImage img2(QFINDTESTDATA(u"xcursor/wait_%1_2.png"_s.arg(reference)));
        img2.convertTo(aniFrame.format());

        QVERIFY(imgEquals(aniFrame, img2));

        // Would be nice to have a cursor with variable delay and hotspot :-)
        QCOMPARE(reader.nextImageDelay(), 40);
        QCOMPARE(reader.text(u"HotspotX"_s), u"48"_s);
        QCOMPARE(reader.text(u"HotspotY"_s), u"48"_s);

        QVERIFY(reader.canRead());
        QCOMPARE(reader.currentImageNumber(), 2);

        QVERIFY(reader.read(&aniFrame));
        QImage img3(QFINDTESTDATA(u"xcursor/wait_%1_3.png"_s.arg(reference)));
        img3.convertTo(aniFrame.format());

        QVERIFY(imgEquals(aniFrame, img3));

        QCOMPARE(reader.text(u"HotspotX"_s), u"48"_s);
        QCOMPARE(reader.text(u"HotspotY"_s), u"48"_s);
        QCOMPARE(reader.nextImageDelay(), 40);

        QVERIFY(reader.canRead());
        QCOMPARE(reader.currentImageNumber(), 3);
    }
};

QTEST_MAIN(XCursorTests)

#include "xcursortest.moc"
