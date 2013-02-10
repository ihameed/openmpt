#include "stdafx.h"

#include "util.h"

namespace modplug {
namespace gui {
namespace qt5 {

size_t color_table_size(BITMAPINFOHEADER &header) {
    switch (header.biBitCount) {
    case 1: return 2;
    case 4: return 16;
    case 8: return 256;

    case 16:
    case 24:
    case 32: return header.biClrUsed;

    default:
        DEBUG_FUNC("invalid bpp = %d", header.biBitCount);
        return 0;
    }
}

// HACK: compatibility shim for a feature that hasn't yet been exposed in Qt 5
enum HBitmapFormat
{
    HBitmapNoAlpha,
    HBitmapPremultipliedAlpha,
    HBitmapAlpha
};

// HACK: compatibility shim for a feature that hasn't yet been exposed in Qt 5
QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int hbitmapFormat = 0) {
    // Verify size
    BITMAP bitmap_info;
    memset(&bitmap_info, 0, sizeof(BITMAP));

    const int res = GetObject(bitmap, sizeof(BITMAP), &bitmap_info);
    if (!res) {
        qErrnoWarning("QPixmap::fromWinHBITMAP(), failed to get bitmap info");
        return QPixmap();
    }
    const int w = bitmap_info.bmWidth;
    const int h = bitmap_info.bmHeight;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = w * h * 4;

    // Get bitmap bits
    QScopedArrayPointer<uchar> data(new uchar[bmi.bmiHeader.biSizeImage]);
    HDC display_dc = GetDC(0);
    if (!GetDIBits(display_dc, bitmap, 0, h, data.data(), &bmi, DIB_RGB_COLORS)) {
        ReleaseDC(0, display_dc);
        qWarning("%s, failed to get bitmap bits", __FUNCTION__);
        return QPixmap();
    }

    QImage::Format imageFormat = QImage::Format_ARGB32_Premultiplied;
    uint mask = 0;
    if (hbitmapFormat == HBitmapNoAlpha) {
        imageFormat = QImage::Format_RGB32;
        mask = 0xff000000;
    }

    // Create image and copy data into image.
    QImage image(w, h, imageFormat);
    if (image.isNull()) { // failed to alloc?
        ReleaseDC(0, display_dc);
        qWarning("%s, failed create image of %dx%d", __FUNCTION__, w, h);
        return QPixmap();
    }
    const int bytes_per_line = w * sizeof(QRgb);
    for (int y = 0; y < h; ++y) {
        QRgb *dest = (QRgb *) image.scanLine(y);
        const QRgb *src = (const QRgb *) (data.data() + y * bytes_per_line);
        for (int x = 0; x < w; ++x) {
            const uint pixel = src[x];
            if ((pixel & 0xff000000) == 0 && (pixel & 0x00ffffff) != 0)
                dest[x] = pixel | 0xff000000;
            else
                dest[x] = pixel | mask;
        }
    }
    ReleaseDC(0, display_dc);
    return QPixmap::fromImage(image);
}


QImage load_bmp_resource(LPTSTR resource_name, HINSTANCE hinstance, HDC hdc) {
    auto hrsrc   = FindResource(hinstance, resource_name, RT_BITMAP);
    auto hglobal = LoadResource(hinstance, hrsrc);
    auto info    = (LPBITMAPINFO) LockResource(hglobal);

    if (!info) {
        DEBUG_FUNC("failed to load resource \"%s\"", resource_name);
        return QPixmap().toImage();
    }

    auto &header     = info->bmiHeader;
    const void *data = &info->bmiColors + color_table_size(header);
    auto hbitmap     = CreateDIBitmap(hdc, &header, CBM_INIT, data, info,
                                      DIB_RGB_COLORS);
    QPixmap pixmap   = qt_pixmapFromWinHBITMAP(hbitmap, HBitmapNoAlpha);
    DeleteObject(hbitmap);

    DEBUG_FUNC("pixmap colorcount = %d, width = %d, height = %d",
               pixmap.colorCount(), pixmap.width(), pixmap.height());

    return pixmap.toImage();
}

}
}
}