#ifndef IMAGEIO_PNGIO_H
#define IMAGEIO_PNGIO_H

#include "bitmap.h"
#include "native-bitmap.h"

namespace ImageIO {
    NativeBitmap loadPNG(const char* filename);

    void savePNG(const char* filename, const Bitmap &bitmap, Properties props);
}

#endif //IMAGEIO_PNGIO_H