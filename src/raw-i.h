#ifndef IMAGEIO_RAW_H
#define IMAGEIO_RAW_H

#include "native-bitmap.h"

namespace ImageIO {
    bool isRAW(const char* filename);

    NativeBitmap loadRAW(const char* file);
}


#endif //IMAGEIO_RAW_H