# Tests

Command line tools to try the plugins by hand. They are built with the
autotests (when `BUILD_TESTING` is on) into the `bin` folder of the build
directory, load the plugins built there but they do not run automatically
by CI.

## imageconverter

Converts an image from one format to another, to check how a plugin reads
or writes it.

```sh
imageconverter image.psd image.png
imageconverter -q 80 image.png image.avif
```

`-l` lists the formats it can read and write.

## imagedump

Writes the raw pixel data of an image (`QImage::bits()`) to a file, to
compare what a plugin reads byte by byte.

```sh
imagedump image.tga image.data
imagedump -q RGBA8888 image.tga image.data
```

`-q` converts the image to a `QImage` format first; `-p` lists them.

## colorspacetest

Saves RGB, gray and CMYK images with several pixel formats and color spaces
in one format, reads them back and reports where they don't match. Gray
images with RGB color spaces are also tested, but only to see that nothing
crashes or leaks: their results are not counted as errors.

```sh
colorspacetest jxl
colorspacetest --verbose --dump jxl-failures jxl
```

`--verbose` prints every case, not only the failed ones, and `--dump` saves
the files of the failed cases in a folder. The exit code is 1 when there are
errors.

`-q 100` tests only lossless compression (by default lossless and
lossy quality 80 is tested)

By default, colorspacetest considers RGB images with no color space
as sRGB images when comparing them.
However, when no color space is attached, some plugins (for example
JXR, DDS) handle floating point pixel formats like linear sRGB while
integer pixel formats are handled as standard (non-linear) sRGB.
Plugins where this behavior is expected should be tested with
`--linear-hdr` option.

**Examples of verbose output from colorspacetest:**

QOI: 16-bin grayscale with linear colorspace written; plug-in reads 8-bit RGB
with linear colorspace. It is expected because of the limitation of the
QOI format. The QOI plugin doesn’t support setting quality
(it is lossless by default).

```
QUALITY PIXEL FORMAT  COLOR SPACE        RESULT       READ AS             READ COLOR SPACE      MAX   MEAN  BLOCK
-       Grayscale16   gray/linear        ok           RGB32               sRGB/linear           0.0    0.0    0.0
```

AVIF: saved P3/gamma2.8 and the same colorspace was loaded. Because of
the lossy compression (quality=90), there is a small difference
in pixel values.
32-bit float pixel format was written, because of the limitation of
the format, only 16-bit depth image can be read.

```
QUALITY PIXEL FORMAT  COLOR SPACE        RESULT       READ AS             READ COLOR SPACE      MAX   MEAN  BLOCK
90      RGBX32FPx4    P3/gamma2.8        ok           RGBX64              same                  6.0    0.8    3.0
```

Examples of possible errors found by colorspacetest:

MISMATCH - plugin attempted to save gray/HLG but loaded gray/table instead.
Images converted to sRGB do not match each other (difference 12 values),
which could be a problem when lossless is expected.
Even in situations when the pixel values are saved losslessly, failure
to encode colorpace results into different visual representation of the image.

CS-IGNORED - plugin was not able to preserve image in CMYK colorspace.
This problem may occur when a plugin tries to save converted RGB values
but adds CMYK colorspace from original image. This combination is
typically not allowed by underlying image libraries.

```
QUALITY PIXEL FORMAT  COLOR SPACE        RESULT       READ AS             READ COLOR SPACE      MAX   MEAN  BLOCK
100     Grayscale16   gray/PQ            MISMATCH     Grayscale16         gray/table           15.0    2.4    7.5
100     Grayscale16   gray/HLG           MISMATCH     Grayscale16         gray/table           12.0    1.4    9.3
24 cases: 20 ok, 4 mismatches, 0 color spaces ignored, 0 read errors, 0 write failed, 0 skipped

CMYK images without and with a CMYK color space
QUALITY PIXEL FORMAT  COLOR SPACE        RESULT       READ AS             READ COLOR SPACE      MAX   MEAN  BLOCK
100     CMYK8888      none               ok           RGB32               same                  0.0    0.0    0.0
100     CMYK8888      CMYK/CGATS001      CS-IGNORED   RGB32               none                 80.0   17.3   66.5
```

**Tip how to check memory leaks in the plugins:**

`valgrind --leak-check=full bin/colorspacetest --size 64x128 -q 95 --verbose hej2`
