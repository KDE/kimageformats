/*
    SPDX-FileCopyrightText: 2026 Ana Bobillo <anabobillo3@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

/*
 * Saves RGB, grayscale and CMYK images without alpha in several pixel formats
 * and color spaces (primaries and transfer functions) with the format given on
 * the command line, reads them back and reports where they do not match.
 *
 * The cases are in four areas:
 * - RGB images with RGB color spaces;
 * - gray images with gray color spaces;
 * - CMYK images without color space and with a CMYK color space made from an
 *   ICC profile (CGATS001Compat-v2-micro.icc, from Compact ICC Profiles by
 *   Clinton Ingram, CC0: a small profile modeled on U.S. Web Coated (SWOP) v2
 *   that only converts from CMYK, which Qt's own tests also use);
 * - gray images with RGB color spaces. Qt accepts them but most software does
 *   not, so a writer or a reader may refuse them: these results are only
 *   informative and are not counted as errors. They are run to see that the
 *   plugin does not crash or leak memory with them.
 *
 * The images are compared in 8 bits after converting both to sRGB, so a format
 * that cannot store a color space passes when it converts the image to sRGB
 * before saving it, and is reported as CS-IGNORED when it saves the values as
 * they are. A CMYK image without color space is converted as Qt converts it to
 * RGB, and one read back as CMYK with the color space it was saved with (or
 * without one, as saved) is compared by its CMYK values. An image read back in
 * another color space or with less depth also passes when its values are those
 * of the source converted to that color space and depth. Lossy cases (quality
 * below 100) compare the mean difference and the averages of 2x2 blocks, so
 * chroma subsampling is not a mismatch.
 *
 * Each group of cases (quality and pixel format) of the first three areas
 * starts with its baseline, the sRGB image or the CMYK image without color
 * space: if it does not round trip, the format cannot store that pixel format
 * and the rest of the group is skipped; if it only does within the lossy
 * tolerances at the highest quality, the group is compared as lossy. The
 * informative cases of a gray pixel format are compared as its gray group.
 *
 * It is not run by CI.
 */

#include <QBuffer>
#include <QColor>
#include <QColorSpace>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QMap>
#include <QTextStream>

#include <algorithm>
#include <cmath>

#include "format-enum.h"

namespace
{

struct ColorSpaceCase {
    QString name;
    QColorSpace colorSpace; // invalid: the image has no color space
};

struct TransferFunctionCase {
    QString name;
    QColorSpace::TransferFunction transferFunction;
    float gamma;
};

const QString baselineRgb = QStringLiteral("sRGB/sRGB");
const QString baselineGray = QStringLiteral("gray/sRGB");

QList<uint16_t> generateCustomTable()
{
    // a curve that no parametric transfer function describes
    QList<uint16_t> table;
    for (int i = 0; i < 1024; ++i) {
        const double x = i / 1023.0;
        table.append(uint16_t(std::lround((x * x + x * x * x) / 2 * 65535)));
    }
    return table;
}

const QList<uint16_t> &customTable()
{
    static const QList<uint16_t> table(generateCustomTable());
    return table;
}

const QList<TransferFunctionCase> &transferFunctions()
{
    static const QList<TransferFunctionCase> transfer_functions = {
        {QStringLiteral("sRGB"), QColorSpace::TransferFunction::SRgb, 0.0f},
        {QStringLiteral("linear"), QColorSpace::TransferFunction::Linear, 0.0f},
        {QStringLiteral("gamma1.8"), QColorSpace::TransferFunction::Gamma, 1.8f},
        {QStringLiteral("gamma2.2"), QColorSpace::TransferFunction::Gamma, 2.2f},
        {QStringLiteral("gamma2.4"), QColorSpace::TransferFunction::Gamma, 2.4f},
        {QStringLiteral("gamma2.8"), QColorSpace::TransferFunction::Gamma, 2.8f},
        {QStringLiteral("ProPhoto"), QColorSpace::TransferFunction::ProPhotoRgb, 0.0f},
        {QStringLiteral("BT2020"), QColorSpace::TransferFunction::Bt2020, 0.0f},
        {QStringLiteral("PQ"), QColorSpace::TransferFunction::St2084, 0.0f},
        {QStringLiteral("HLG"), QColorSpace::TransferFunction::Hlg, 0.0f},
        {QStringLiteral("table"), QColorSpace::TransferFunction::Custom, 0.0f},
    };
    return transfer_functions;
}

QColorSpace rgbColorSpace(QColorSpace::Primaries primaries, const TransferFunctionCase &tf)
{
    if (tf.transferFunction == QColorSpace::TransferFunction::Custom) {
        return QColorSpace(primaries, customTable());
    }
    return QColorSpace(primaries, tf.transferFunction, tf.gamma);
}

const QPointF &d65()
{
    static const QPointF d65_whitepoint(QColorSpace(QColorSpace::SRgb).whitePoint());
    return d65_whitepoint;
}

QColorSpace grayColorSpace(const TransferFunctionCase &tf)
{
    if (tf.transferFunction == QColorSpace::TransferFunction::Custom) {
        return QColorSpace(d65(), customTable());
    }
    return QColorSpace(d65(), tf.transferFunction, tf.gamma);
}

// Every RGB color space, the baseline first.
QList<ColorSpaceCase> rgbColorSpaces()
{
    const QList<QPair<QString, QColorSpace::Primaries>> primaries = {
        {QStringLiteral("sRGB"), QColorSpace::Primaries::SRgb},
        {QStringLiteral("P3"), QColorSpace::Primaries::DciP3D65},
        {QStringLiteral("AdobeRGB"), QColorSpace::Primaries::AdobeRgb},
        {QStringLiteral("BT2020"), QColorSpace::Primaries::Bt2020},
        {QStringLiteral("ProPhoto"), QColorSpace::Primaries::ProPhotoRgb},
    };
    QList<ColorSpaceCase> list;
    list.append({baselineRgb, QColorSpace(QColorSpace::SRgb)});
    for (const auto &p : primaries) {
        for (const auto &tf : transferFunctions()) {
            const QString name = p.first + QLatin1Char('/') + tf.name;
            if (name != baselineRgb) {
                list.append({name, rgbColorSpace(p.second, tf)});
            }
        }
    }
    return list;
}

// Every gray color space, the baseline first.
QList<ColorSpaceCase> grayColorSpaces()
{
    QList<ColorSpaceCase> list;
    list.append({baselineGray, grayColorSpace(transferFunctions().first())});
    for (const auto &tf : transferFunctions()) {
        const QString name = QStringLiteral("gray/") + tf.name;
        if (name != baselineGray) {
            list.append({name, grayColorSpace(tf)});
        }
    }
    return list;
}

// The baseline, then the image without color space, then the others.
QList<ColorSpaceCase> withoutColorSpace(QList<ColorSpaceCase> list)
{
    list.insert(1, {QStringLiteral("none"), QColorSpace()});
    return list;
}

QColorSpace cmykColorSpace()
{
    QFile file(QStringLiteral(":/CGATS001Compat-v2-micro.icc"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QColorSpace::fromIccProfile(file.readAll());
}

// The baseline, the image without color space, then the one with the ICC profile.
QList<ColorSpaceCase> cmykColorSpaces(const QColorSpace &profile)
{
    return {
        {QStringLiteral("none"), QColorSpace()},
        {QStringLiteral("CMYK/CGATS001"), profile},
    };
}

bool isGray(QImage::Format format)
{
    return QImage::toPixelFormat(format).colorModel() == QPixelFormat::Grayscale;
}

bool isCmyk(QImage::Format format)
{
    return QImage::toPixelFormat(format).colorModel() == QPixelFormat::CMYK;
}

/*
 * The test images are made in sRGB and converted to the color space of each
 * case, so their colors fit in every gamut and a format that converts to sRGB
 * loses only precision. They have no sharp edges, so lossy compression changes
 * them little.
 */
QImage rgbPattern(const QSize &size)
{
    QImage image(size, QImage::Format_RGBX32FPx4);
    const int w = size.width();
    const int h = size.height();
    for (int y = 0; y < h; ++y) {
        auto line = reinterpret_cast<float *>(image.scanLine(y));
        const float v = h > 1 ? float(y) / (h - 1) : 0.0f;
        for (int x = 0; x < w; ++x) {
            const float u = w > 1 ? float(x) / (w - 1) : 0.0f;
            // the top row is a gray ramp from black to white, the bottom one all hues
            float r = 0;
            float g = 0;
            float b = 0;
            const float value = (1.0f - v) * u + v * (0.35f + 0.6f * u);
            QColor::fromHsvF(u * 0.999f, 0.8f * v, value).getRgbF(&r, &g, &b);
            line[x * 4 + 0] = r;
            line[x * 4 + 1] = g;
            line[x * 4 + 2] = b;
            line[x * 4 + 3] = 1.0f;
        }
    }
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    return image;
}

QImage grayPattern(const QSize &size)
{
    QImage image(size, QImage::Format_Grayscale16);
    const int w = size.width();
    const int h = size.height();
    for (int y = 0; y < h; ++y) {
        auto line = reinterpret_cast<quint16 *>(image.scanLine(y));
        const double v = h > 1 ? double(y) / (h - 1) : 0.0;
        for (int x = 0; x < w; ++x) {
            const double u = w > 1 ? double(x) / (w - 1) : 0.0;
            line[x] = quint16(std::lround((0.8 * u + 0.2 * v) * 65535));
        }
    }
    image.setColorSpace(QColorSpace(d65(), QColorSpace::TransferFunction::SRgb));
    return image;
}

// Made in CMYK, without color space, as the profile only converts from CMYK.
QImage cmykPattern(const QSize &size)
{
    QImage image(size, QImage::Format_CMYK8888);
    const int w = size.width();
    const int h = size.height();
    for (int y = 0; y < h; ++y) {
        auto line = image.scanLine(y);
        const double v = h > 1 ? double(y) / (h - 1) : 0.0;
        for (int x = 0; x < w; ++x) {
            const double u = w > 1 ? double(x) / (w - 1) : 0.0;
            // the top row is a black ramp, the bottom one goes from cyan through magenta to yellow
            const double cmyk[4] = {
                0.9 * v * (1.0 - u),
                0.9 * v * (1.0 - std::abs(2.0 * u - 1.0)),
                0.9 * v * u,
                0.9 * (1.0 - v) * u + 0.1 * v * u,
            };
            for (int c = 0; c < 4; ++c) {
                line[x * 4 + c] = uchar(std::lround(cmyk[c] * 255));
            }
        }
    }
    return image;
}

QImage sourceImage(const QImage &pattern, QImage::Format format, const QColorSpace &colorSpace)
{
    if (!colorSpace.isValid() || !pattern.colorSpace().isValid()) {
        // without color space, or the CMYK pattern, whose values are taken as they are in the color space
        QImage image = pattern.convertedTo(format);
        image.setColorSpace(colorSpace);
        return image;
    }
    return pattern.convertedToColorSpace(colorSpace, format);
}

QColorSpace graySRgb()
{
    return QColorSpace(d65(), QColorSpace::TransferFunction::SRgb);
}

// Images without color space are taken as sRGB, except CMYK ones (none: compared as Qt converts
// them to RGB).
QColorSpace assumedColorSpace(const QImage &image, bool prefer_linear_hdr)
{
    if (image.colorSpace().isValid()) {
        return image.colorSpace();
    }
    if (isCmyk(image.format())) {
        return {};
    }

    if (isGray(image.format())) {
        return graySRgb();
    }

    if (!prefer_linear_hdr) {
        return QColorSpace(QColorSpace::SRgb);
    }

    switch (image.format()) {
    case QImage::Format_RGBX16FPx4:
    case QImage::Format_RGBX32FPx4:
        return QColorSpace(QColorSpace::SRgbLinear);
        break;
    default:
        return QColorSpace(QColorSpace::SRgb);
        break;
    }
}

QImage withColorSpace(QImage image, const QColorSpace &colorSpace)
{
    image.setColorSpace(colorSpace);
    return image;
}

// The values of an image as they are, in 8 bits.
QImage raw(const QImage &image)
{
    return withColorSpace(image, QColorSpace()).convertedTo(QImage::Format_ARGB32);
}

// What an image is compared as: 8-bit sRGB. The alpha is kept, so a reader that adds transparency is
// seen.
QImage comparable(const QImage &image, bool prefer_linear_hdr)
{
    const QColorSpace colorSpace = assumedColorSpace(image, prefer_linear_hdr);
    if (!colorSpace.isValid()) {
        return raw(image);
    }
    QImage converted = withColorSpace(image, colorSpace).convertedToColorSpace(QColorSpace(QColorSpace::SRgb), QImage::Format_ARGB32);
    if (converted.isNull()) {
        // a color space Qt cannot convert from: compare the raw values
        converted = raw(image);
    }
    return converted;
}

/*
 * The source converted to the color space and pixel format of the image read back: what a writer
 * that saves the image in that color space and depth gives at best. Null when that conversion
 * would hide a loss (colors saved as gray).
 */
QImage convertedLike(const QImage &source, const QImage &read, bool prefer_linear_hdr)
{
    if (!isGray(source.format()) && isGray(read.format())) {
        return {};
    }
    const QColorSpace from = assumedColorSpace(source, prefer_linear_hdr);
    const QColorSpace to = assumedColorSpace(read, prefer_linear_hdr);
    // no conversion from or to a CMYK image without color space, nor to a CMYK color space that only
    // converts from CMYK
    if (!from.isValid() || !to.isValid() || !to.isValidTarget() || isCmyk(read.format()) != (to.colorModel() == QColorSpace::ColorModel::Cmyk)) {
        return {};
    }
    QImage::Format format = read.format();
    if (read.depth() <= 8 && !isGray(format)) {
        // a palette: its colors have 8 bits
        format = read.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    }
    return withColorSpace(source, from).convertedToColorSpace(to, format);
}

struct Diff {
    double max = 0; // largest difference of a channel of a pixel
    double mean = 0; // average difference of the channel that differs most
    double block = 0; // 99th percentile of the differences of the 2x2 blocks
};

int channel(QRgb pixel, int c)
{
    switch (c) {
    case 0:
        return qRed(pixel);
    case 1:
        return qGreen(pixel);
    case 2:
        return qBlue(pixel);
    }
    return qAlpha(pixel);
}

// Differences in 8-bit levels of two ARGB32 images, or of two CMYK8888 ones.
Diff compare(const QImage &expected, const QImage &actual)
{
    Diff diff;
    const int w = expected.width();
    const int h = expected.height();
    double sums[4] = {};
    QList<double> blocks;
    for (int y = 0; y < h; y += 2) {
        for (int x = 0; x < w; x += 2) {
            int blockExpected[4] = {};
            int blockActual[4] = {};
            int n = 0;
            for (int by = y; by < std::min(y + 2, h); ++by) {
                auto e = reinterpret_cast<const QRgb *>(expected.constScanLine(by));
                auto a = reinterpret_cast<const QRgb *>(actual.constScanLine(by));
                for (int bx = x; bx < std::min(x + 2, w); ++bx) {
                    for (int c = 0; c < 4; ++c) {
                        const int pe = channel(e[bx], c);
                        const int pa = channel(a[bx], c);
                        const int d = std::abs(pe - pa);
                        diff.max = std::max(diff.max, double(d));
                        sums[c] += d;
                        blockExpected[c] += pe;
                        blockActual[c] += pa;
                    }
                    ++n;
                }
            }
            double block = 0;
            for (int c = 0; c < 4; ++c) {
                block = std::max(block, std::abs(blockExpected[c] - blockActual[c]) / double(n));
            }
            blocks.append(block);
        }
    }
    if (!blocks.isEmpty()) {
        std::sort(blocks.begin(), blocks.end());
        diff.block = blocks.at(std::min(blocks.size() - 1, qsizetype(blocks.size() * 0.99)));
    }
    if (w * h > 0) {
        diff.mean = *std::max_element(std::begin(sums), std::end(sums)) / (double(w) * h);
    }
    return diff;
}

enum class Mode {
    Unknown,
    Lossless,
    Lossy,
};

struct Tolerances {
    double lossless = 2; // max
    double lossyBlock = 24; // block
    double lossyMean = 4; // mean
};

bool matches(const Diff &diff, Mode mode, const Tolerances &tol)
{
    if (mode == Mode::Lossless) {
        return diff.max <= tol.lossless;
    }
    return diff.block <= tol.lossyBlock && diff.mean <= tol.lossyMean;
}

enum class Result {
    Ok,
    Mismatch, // error
    ReadError, // error
    SizeError, // error
    WriteFailed, // the writer refused the image: not an error
    ColorSpaceIgnored, // error: saved without color space and without converting it to sRGB
    NoRoundTrip, // the baseline does not round trip: the group is skipped
    SourceError, // the test image could not be made
};

QString resultName(Result result)
{
    switch (result) {
    case Result::Ok:
        return QStringLiteral("ok");
    case Result::Mismatch:
        return QStringLiteral("MISMATCH");
    case Result::ReadError:
        return QStringLiteral("READ-ERROR");
    case Result::SizeError:
        return QStringLiteral("SIZE-ERROR");
    case Result::WriteFailed:
        return QStringLiteral("write-failed");
    case Result::ColorSpaceIgnored:
        return QStringLiteral("CS-IGNORED");
    case Result::NoRoundTrip:
        return QStringLiteral("no-roundtrip");
    case Result::SourceError:
        return QStringLiteral("no-source");
    }
    return {};
}

bool isError(Result result)
{
    return result == Result::Mismatch || result == Result::ReadError || result == Result::SizeError || result == Result::ColorSpaceIgnored;
}

QString describe(const QColorSpace &cs)
{
    if (!cs.isValid()) {
        return QStringLiteral("none");
    }
    if (cs.colorModel() == QColorSpace::ColorModel::Cmyk) {
        return QStringLiteral("CMYK");
    }
    if (cs.transformModel() != QColorSpace::TransformModel::ThreeComponentMatrix) {
        return QStringLiteral("ICC-LUT");
    }
    QString tf;
    switch (cs.transferFunction()) {
    case QColorSpace::TransferFunction::Linear:
        tf = QStringLiteral("linear");
        break;
    case QColorSpace::TransferFunction::Gamma:
        tf = QStringLiteral("gamma") + QString::number(cs.gamma(), 'g', 3);
        break;
    case QColorSpace::TransferFunction::SRgb:
        tf = QStringLiteral("sRGB");
        break;
    case QColorSpace::TransferFunction::ProPhotoRgb:
        tf = QStringLiteral("ProPhoto");
        break;
    case QColorSpace::TransferFunction::Bt2020:
        tf = QStringLiteral("BT2020");
        break;
    case QColorSpace::TransferFunction::St2084:
        tf = QStringLiteral("PQ");
        break;
    case QColorSpace::TransferFunction::Hlg:
        tf = QStringLiteral("HLG");
        break;
    case QColorSpace::TransferFunction::Custom:
        tf = QStringLiteral("table");
        break;
    }
    if (cs.colorModel() == QColorSpace::ColorModel::Gray) {
        return QStringLiteral("gray/") + tf;
    }
    QString primaries;
    switch (cs.primaries()) {
    case QColorSpace::Primaries::SRgb:
        primaries = QStringLiteral("sRGB");
        break;
    case QColorSpace::Primaries::DciP3D65:
        primaries = QStringLiteral("P3");
        break;
    case QColorSpace::Primaries::AdobeRgb:
        primaries = QStringLiteral("AdobeRGB");
        break;
    case QColorSpace::Primaries::Bt2020:
        primaries = QStringLiteral("BT2020");
        break;
    case QColorSpace::Primaries::ProPhotoRgb:
        primaries = QStringLiteral("ProPhoto");
        break;
    case QColorSpace::Primaries::Custom:
        primaries = QStringLiteral("custom");
        break;
    }
    return primaries + QLatin1Char('/') + tf;
}

struct Outcome {
    Result result = Result::Ok;
    Diff diff;
    QString readFormat;
    QString readColorSpace;
    QString error;
    QByteArray data;
    QImage expected;
    QImage actual;
};

struct Options {
    QByteArray format;
    QSize size = QSize(67, 43);
    Tolerances tolerances;
    QDir dumpDir;
    bool dump = false;
    bool verbose = false;
    bool assume_linear_hdr = false;
};

Outcome runCase(int quality, const QImage &source, Mode mode, const Options &opt)
{
    Outcome out;
    {
        QBuffer buffer(&out.data);
        QImageWriter writer(&buffer, opt.format);
        if (quality >= 0) {
            writer.setQuality(quality);
        }
        if (!writer.write(source)) {
            out.result = Result::WriteFailed;
            out.error = writer.errorString();
            return out;
        }
    }

    QImage read;
    {
        QBuffer buffer(&out.data);
        QImageReader reader(&buffer, opt.format);
        if (!reader.read(&read)) {
            out.result = Result::ReadError;
            out.error = reader.errorString();
            return out;
        }
    }
    out.readFormat = formatToString(read.format());
    out.readColorSpace = read.colorSpace() == source.colorSpace() ? QStringLiteral("same") : describe(read.colorSpace());
    if (read.size() != source.size()) {
        out.result = Result::SizeError;
        out.error = QStringLiteral("read back as %1x%2").arg(read.width()).arg(read.height());
        return out;
    }

    if (isCmyk(source.format()) && isCmyk(read.format()) && read.colorSpace() == source.colorSpace()) {
        // the same CMYK color space, or none: the CMYK values are compared
        out.expected = withColorSpace(source, QColorSpace());
        out.actual = withColorSpace(read, QColorSpace());
    } else {
        out.expected = comparable(source, opt.assume_linear_hdr);
        out.actual = comparable(read, opt.assume_linear_hdr);
    }
    out.diff = compare(out.expected, out.actual);
    const bool match = matches(out.diff, mode, opt.tolerances);

    // Saved with the values as they are, without color space or tagged as sRGB (what many readers
    // assume): it looks different unless the source is sRGB.
    const bool readAsSRgb = !read.colorSpace().isValid() || read.colorSpace() == assumedColorSpace(withColorSpace(read, QColorSpace()), opt.assume_linear_hdr);
    if (readAsSRgb && source.colorSpace().isValid() && source.colorSpace() != read.colorSpace()) {
        const Diff asIs = compare(raw(source), raw(read));
        if (matches(asIs, mode, opt.tolerances) && (!match || asIs.mean + 0.5 < out.diff.mean)) {
            out.result = Result::ColorSpaceIgnored;
            return out;
        }
    }
    if (match) {
        return out;
    }

    // Saved in another color space, or with less depth: the source converted to them is the best it
    // can be. Compared as the values read, so that rounding them is not taken for a mismatch.
    const QImage readValues = raw(read);
    const QImage converted = convertedLike(source, read, opt.assume_linear_hdr);
    if (!converted.isNull()) {
        const Diff diff = compare(raw(converted), readValues);
        if (matches(diff, mode, opt.tolerances)) {
            out.diff = diff;
            out.expected = raw(converted);
            out.actual = readValues;
            return out;
        }
    }

    out.result = Result::Mismatch;
    return out;
}

QString caseName(int quality, QImage::Format pixelFormat, const QString &colorSpace)
{
    QString name = QStringLiteral("q%1-%2-%3").arg(quality < 0 ? QStringLiteral("default") : QString::number(quality), formatToString(pixelFormat), colorSpace);
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    return name;
}

void dumpCase(const Options &opt, const QString &name, const Outcome &out)
{
    if (!opt.dump || out.data.isEmpty()) {
        return;
    }
    QFile f(opt.dumpDir.filePath(name + QLatin1Char('.') + QString::fromLatin1(opt.format)));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(out.data);
    }
    if (!out.expected.isNull()) {
        out.expected.save(opt.dumpDir.filePath(name + QStringLiteral("-expected.png")), "png");
    }
    if (!out.actual.isNull()) {
        out.actual.save(opt.dumpDir.filePath(name + QStringLiteral("-actual.png")), "png");
    }
}

QString column(const QString &text, int width)
{
    return text.leftJustified(width - 1) + QLatin1Char(' ');
}

QString number(double value)
{
    return QString::number(value, 'f', 1).rightJustified(6) + QLatin1Char(' ');
}

void printHeader(QTextStream &out)
{
    out << column(QStringLiteral("QUALITY"), 8) << column(QStringLiteral("PIXEL FORMAT"), 14) << column(QStringLiteral("COLOR SPACE"), 19)
        << column(QStringLiteral("RESULT"), 13) << column(QStringLiteral("READ AS"), 20) << column(QStringLiteral("READ COLOR SPACE"), 19)
        << QStringLiteral("   MAX   MEAN  BLOCK") << '\n';
}

QString qualityText(int quality, Mode mode)
{
    QString q = quality < 0 ? QStringLiteral("-") : QString::number(quality);
    if (mode == Mode::Lossy && (quality < 0 || quality == 100)) {
        // lossy even at the highest quality
        q += QStringLiteral(" lossy");
    }
    return q;
}

// An informative result is written in lowercase: it is not an error.
void printRow(QTextStream &out, int quality, Mode mode, QImage::Format pixelFormat, const QString &colorSpace, const Outcome &o, bool informative)
{
    out << column(qualityText(quality, mode), 8) << column(formatToString(pixelFormat), 14) << column(colorSpace, 19)
        << column(informative ? resultName(o.result).toLower() : resultName(o.result), 13);
    if (o.error.isEmpty()
        && (o.result == Result::Ok || o.result == Result::Mismatch || o.result == Result::ColorSpaceIgnored || o.result == Result::NoRoundTrip)) {
        out << column(o.readFormat, 20) << column(o.readColorSpace, 19) << number(o.diff.max) << number(o.diff.mean) << number(o.diff.block);
    } else if (!o.error.isEmpty()) {
        out << o.error;
    }
    out << '\n';
    out.flush();
}

struct Totals {
    int cases = 0;
    int ok = 0;
    int mismatches = 0;
    int ignored = 0;
    int readErrors = 0;
    int writeFailed = 0;
    int skipped = 0;

    void add(Result r)
    {
        ++cases;
        switch (r) {
        case Result::Ok:
            ++ok;
            break;
        case Result::Mismatch:
            ++mismatches;
            break;
        case Result::ColorSpaceIgnored:
            ++ignored;
            break;
        case Result::ReadError:
        case Result::SizeError:
            ++readErrors;
            break;
        case Result::WriteFailed:
            ++writeFailed;
            break;
        case Result::NoRoundTrip:
        case Result::SourceError:
            ++skipped;
            break;
        }
    }
    void add(const Totals &t)
    {
        cases += t.cases;
        ok += t.ok;
        mismatches += t.mismatches;
        ignored += t.ignored;
        readErrors += t.readErrors;
        writeFailed += t.writeFailed;
        skipped += t.skipped;
    }
    int errors() const
    {
        return mismatches + ignored + readErrors;
    }
    QString text() const
    {
        return QStringLiteral("%1 cases: %2 ok, %3 mismatches, %4 color spaces ignored, %5 read errors, %6 write failed, %7 skipped")
            .arg(cases)
            .arg(ok)
            .arg(mismatches)
            .arg(ignored)
            .arg(readErrors)
            .arg(writeFailed)
            .arg(skipped);
    }
};

struct Area {
    QString title;
    QList<QImage::Format> pixelFormats;
    QImage pattern;
    QList<ColorSpaceCase> colorSpaces; // the first one is the baseline of each group, unless informative
    bool informative; // no baseline, and its results are not errors
    Totals totals;
};

/*
 * Runs the cases of one quality and pixel format of an area, and returns how they were compared. An
 * informative group is compared with the mode given and has no baseline.
 */
Mode runGroup(QTextStream &out, Area &area, int quality, QImage::Format pixelFormat, Mode mode, const Options &opt)
{
    if (mode == Mode::Unknown && quality >= 0 && quality < 100) {
        // a quality below 100 is lossy; otherwise the baseline tells
        mode = Mode::Lossy;
    }
    Totals group;
    int ignored = 0;
    for (const auto &c : std::as_const(area.colorSpaces)) {
        const bool baseline = !area.informative && &c == &area.colorSpaces.first();
        const QImage source = sourceImage(area.pattern, pixelFormat, c.colorSpace);
        Outcome o;
        if (source.isNull() || source.format() != pixelFormat || source.colorSpace() != c.colorSpace) {
            o.result = Result::SourceError;
            o.error = QStringLiteral("could not make the test image");
        } else {
            o = runCase(quality, source, mode == Mode::Unknown ? Mode::Lossless : mode, opt);
        }

        bool lossyAtBest = false;
        bool skipGroup = false;
        if (baseline) {
            // decides how the group is compared, or that it cannot be
            if (o.result == Result::Ok) {
                if (mode == Mode::Unknown) {
                    mode = Mode::Lossless;
                }
            } else if (mode == Mode::Unknown && o.result == Result::Mismatch && matches(o.diff, Mode::Lossy, opt.tolerances)) {
                // lossy even at the highest quality
                mode = Mode::Lossy;
                o.result = Result::Ok;
                lossyAtBest = true;
            } else {
                // a file written that cannot be read stays an error
                if (o.result == Result::Mismatch || o.result == Result::SizeError || o.result == Result::ColorSpaceIgnored) {
                    o.result = Result::NoRoundTrip;
                }
                skipGroup = true;
            }
        }

        group.add(o.result);
        if (area.informative) {
            if (opt.verbose) {
                printRow(out, quality, mode, pixelFormat, c.name, o, true);
            }
        } else if (opt.verbose || (baseline && (o.result != Result::Ok || lossyAtBest)) || o.result == Result::WriteFailed || o.result == Result::SourceError
                   || (isError(o.result) && o.result != Result::ColorSpaceIgnored)) {
            printRow(out, quality, mode, pixelFormat, c.name, o, false);
        } else if (o.result == Result::ColorSpaceIgnored) {
            ++ignored;
        }
        if (isError(o.result) || o.result == Result::NoRoundTrip) {
            dumpCase(opt, caseName(quality, pixelFormat, c.name), o);
        }
        if (skipGroup) {
            break;
        }
    }
    if (ignored > 0) {
        // one line for the group, not one for each color space
        Outcome o;
        o.result = Result::ColorSpaceIgnored;
        o.error = QStringLiteral("%1 color spaces saved as sRGB without converting (--verbose lists them)").arg(ignored);
        printRow(out, quality, mode, pixelFormat, QStringLiteral("(%1)").arg(ignored), o, false);
    }
    if (area.informative && !opt.verbose && group.ok != group.cases) {
        // one line for the group
        out << column(qualityText(quality, mode), 8) << column(formatToString(pixelFormat), 14) << group.text() << " (--verbose lists them)\n";
        out.flush();
    }
    area.totals.add(group);
    return mode;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::removeLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::setApplicationName(QStringLiteral("colorspacetest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Saves images with several pixel formats and color spaces in a format, reads them back and reports where the pixels do not match."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("format"), QStringLiteral("the format to test."));
    QCommandLineOption qualitiesOpt(QStringList() << QStringLiteral("q") << QStringLiteral("qualities"),
                                    QStringLiteral("Comma-separated qualities to save with, if the format supports it (default: 100,80)."),
                                    QStringLiteral("qualities"));
    QCommandLineOption sizeOpt(QStringList() << QStringLiteral("size"), QStringLiteral("Size of the test images (default: 67x43)."), QStringLiteral("WxH"));
    QCommandLineOption toleranceOpt(QStringList() << QStringLiteral("t") << QStringLiteral("tolerance"),
                                    QStringLiteral("Largest difference of a pixel channel, in 8-bit levels, when saving lossless (default: 2)."),
                                    QStringLiteral("levels"));
    QCommandLineOption lossyToleranceOpt(QStringList() << QStringLiteral("lossy-tolerance"),
                                         QStringLiteral("Difference of the 2x2 block averages that 99% of the blocks must stay within, in 8-bit levels, "
                                                        "when saving lossy (default: 24); the mean difference of the pixels may be a sixth of it."),
                                         QStringLiteral("levels"));
    QCommandLineOption dumpOpt(QStringList() << QStringLiteral("d") << QStringLiteral("dump"),
                               QStringLiteral("Save the file written and the images compared (sRGB) of each failed case in this folder."),
                               QStringLiteral("folder"));
    QCommandLineOption verboseOpt(QStringList() << QStringLiteral("verbose"), QStringLiteral("Print every case, not only the failed ones."));
    QCommandLineOption linearOpt(QStringList() << QStringLiteral("linear-hdr"),
                                 QStringLiteral("Assume linear colorspace (if not specified) for RGBX16FPx4 and RGBX32FPx4"));

    parser.addOption(qualitiesOpt);
    parser.addOption(sizeOpt);
    parser.addOption(toleranceOpt);
    parser.addOption(lossyToleranceOpt);
    parser.addOption(dumpOpt);
    parser.addOption(verboseOpt);
    parser.addOption(linearOpt);
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);
    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        err << "Give one format to test\n";
        return 2;
    }
    Options opt;
    opt.format = args.first().toLatin1().toLower();
    if (!QImageWriter::supportedImageFormats().contains(opt.format) || !QImageReader::supportedImageFormats().contains(opt.format)) {
        err << "Format " << args.first() << " cannot be both written and read\n";
        return 2;
    }
    opt.verbose = parser.isSet(verboseOpt);

    opt.assume_linear_hdr = parser.isSet(linearOpt);

    QList<int> qualities;
    QStringList qualityNames = parser.value(qualitiesOpt).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (qualityNames.isEmpty()) {
        qualityNames = QStringList{QStringLiteral("100"), QStringLiteral("80")};
    }
    for (const auto &name : std::as_const(qualityNames)) {
        bool ok = false;
        const int value = name.trimmed().toInt(&ok);
        if (!ok || value < 0 || value > 100) {
            err << "Invalid quality " << name << '\n';
            return 2;
        }
        qualities.append(value);
    }
    {
        QByteArray dummy;
        QBuffer buffer(&dummy);
        QImageWriter writer(&buffer, opt.format);
        if (!writer.supportsOption(QImageIOHandler::Quality)) {
            qualities = {-1};
        }
    }

    if (parser.isSet(sizeOpt)) {
        const QStringList wh = parser.value(sizeOpt).split(QLatin1Char('x'));
        bool okw = false;
        bool okh = false;
        if (wh.size() == 2) {
            opt.size = QSize(wh.at(0).toInt(&okw), wh.at(1).toInt(&okh));
        }
        if (!okw || !okh || opt.size.width() < 1 || opt.size.height() < 1 || opt.size.width() > 4096 || opt.size.height() > 4096) {
            err << "Invalid size " << parser.value(sizeOpt) << '\n';
            return 2;
        }
    }
    if (parser.isSet(toleranceOpt)) {
        bool ok = false;
        opt.tolerances.lossless = parser.value(toleranceOpt).toDouble(&ok);
        if (!ok || opt.tolerances.lossless < 0) {
            err << "Invalid tolerance " << parser.value(toleranceOpt) << '\n';
            return 2;
        }
    }
    if (parser.isSet(lossyToleranceOpt)) {
        bool ok = false;
        opt.tolerances.lossyBlock = parser.value(lossyToleranceOpt).toDouble(&ok);
        if (!ok || opt.tolerances.lossyBlock < 0) {
            err << "Invalid lossy tolerance " << parser.value(lossyToleranceOpt) << '\n';
            return 2;
        }
        opt.tolerances.lossyMean = opt.tolerances.lossyBlock / 6;
    }
    if (parser.isSet(dumpOpt)) {
        opt.dump = true;
        opt.dumpDir = QDir(parser.value(dumpOpt));
        if (!opt.dumpDir.mkpath(QStringLiteral("."))) {
            err << "Cannot create " << parser.value(dumpOpt) << '\n';
            return 2;
        }
    }

    const QColorSpace cmykProfile = cmykColorSpace();
    if (!cmykProfile.isValid() || cmykProfile.colorModel() != QColorSpace::ColorModel::Cmyk) {
        err << "Cannot load the CMYK ICC profile\n";
        return 2;
    }

    const QList<QImage::Format> rgbFormats{QImage::Format_RGB32, QImage::Format_RGBX64, QImage::Format_RGBX16FPx4, QImage::Format_RGBX32FPx4};
    const QList<QImage::Format> grayFormats{QImage::Format_Grayscale8, QImage::Format_Grayscale16};
    const QImage rgbImage = rgbPattern(opt.size);
    const QImage grayImage = grayPattern(opt.size);
    Area rgb{QStringLiteral("RGB images with RGB color spaces"), rgbFormats, rgbImage, withoutColorSpace(rgbColorSpaces()), false, {}};
    Area gray{QStringLiteral("Gray images with gray color spaces"), grayFormats, grayImage, withoutColorSpace(grayColorSpaces()), false, {}};
    Area cmyk{QStringLiteral("CMYK images without and with a CMYK color space"),
              {QImage::Format_CMYK8888},
              cmykPattern(opt.size),
              cmykColorSpaces(cmykProfile),
              false,
              {}};
    // Qt accepts them but most other software does not
    Area informative{QStringLiteral("Gray images with RGB color spaces (informative, not counted as errors)"),
                     grayFormats,
                     grayImage,
                     rgbColorSpaces(),
                     true,
                     {}};

    out << "Format " << opt.format << ", plugins from " << QStringLiteral(PLUGIN_DIR) << ", image size " << opt.size.width() << 'x' << opt.size.height()
        << '\n';

    if (opt.assume_linear_hdr) {
        out << "Unspecified colorspace for floating point fixel formats (RGBX16FPx4,RGBX32FPx4) is assumed SRgbLinear,\n";
        out << "Unspecified colorspace for other (integer) RGB formats is assumed non-linear sRGB\n";
    } else {
        out << "Unspecified RGB colorspace is assumed as standard (non-linear) sRGB\n";
    }

    // the informative cases of a gray pixel format are compared as its gray group
    QMap<std::pair<int, int>, Mode> grayModes;
    for (Area *area : {&rgb, &gray, &cmyk, &informative}) {
        out << '\n' << area->title << '\n';
        printHeader(out);
        for (const int quality : std::as_const(qualities)) {
            for (const QImage::Format pixelFormat : std::as_const(area->pixelFormats)) {
                const std::pair<int, int> key(quality, int(pixelFormat));
                const Mode mode = runGroup(out, *area, quality, pixelFormat, area->informative ? grayModes.value(key, Mode::Unknown) : Mode::Unknown, opt);
                if (area == &gray) {
                    grayModes.insert(key, mode);
                }
            }
        }
        out << area->totals.text() << '\n';
        out.flush();
    }

    out << "\nSummary for " << opt.format << '\n';
    for (const Area *area : {&rgb, &gray, &cmyk, &informative}) {
        out << "  " << area->title << ": " << area->totals.text() << '\n';
    }
    const int errors = rgb.totals.errors() + gray.totals.errors() + cmyk.totals.errors();
    out << (errors > 0 ? QStringLiteral("%1 errors\n").arg(errors) : QStringLiteral("No errors\n"));

    return errors > 0 ? 1 : 0;
}
