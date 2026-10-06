#ifndef IMAGEIO_JPEGIO_H
#define IMAGEIO_JPEGIO_H

#include "ImageIO/bitmap.h"

namespace ImageIO {
    Bitmap loadJPEG(const char* filename);

    void saveJPEG(const char* filename, const Bitmap &bitmap, Properties props);
}

#endif //IMAGEIO_JPEGIO_H