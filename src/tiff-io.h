#ifndef IMAGEIO_TIFFIO_H
#define IMAGEIO_TIFFIO_H

#include "bitmap.h"
#include "native-bitmap.h"

namespace ImageIO {
    NativeBitmap loadTIFF(const char* filename);

    void saveTIFF(const char* filename, const Bitmap &bitmap, Properties props);
}

#endif //IMAGEIO_TIFFIO_H