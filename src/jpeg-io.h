#ifndef IMAGEIO_JPEGIO_H
#define IMAGEIO_JPEGIO_H

#include "bitmap.h"
#include "native-bitmap.h"

namespace ImageIO {
    NativeBitmap loadJPEG(const char* filename);

    void saveJPEG(const char* filename, const Bitmap &bitmap, Properties props);
}

#endif //IMAGEIO_JPEGIO_H