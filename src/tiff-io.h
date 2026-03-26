#ifndef IMAGEIO_TIFFIO_H
#define IMAGEIO_TIFFIO_H

#include "bitmap.h"

namespace ImageIO {
    Bitmap loadTIFF(const char* filename);

    void saveTIFF(const char* filename, const Bitmap &bitmap, Properties props);
}

#endif //IMAGEIO_TIFFIO_H