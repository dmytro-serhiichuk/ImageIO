#ifndef IMAGEIO_RAW_H
#define IMAGEIO_RAW_H

#include "bitmap.h"

namespace ImageIO {
    bool isRAW(const char* filename);

    Bitmap loadRAW(const char* file);
}


#endif //IMAGEIO_RAW_H