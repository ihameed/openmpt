#include "stdafx.h"

#include "util.h"

namespace modplug {
namespace gui {
namespace qt4 {

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
    QPixmap pixmap   = QPixmap::fromWinHBITMAP(hbitmap, QPixmap::NoAlpha);
    DeleteObject(hbitmap);

    DEBUG_FUNC("pixmap colorcount = %d, width = %d, height = %d",
               pixmap.colorCount(), pixmap.width(), pixmap.height());

    return pixmap.toImage();
}

}
}
}