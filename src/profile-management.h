#ifndef IMAGEIO_PROFILE_MANAGEMENT_H
#define IMAGEIO_PROFILE_MANAGEMENT_H

#include <lcms2.h>
#include "bitmap.h"

namespace ImageIO {
    cmsHPROFILE cloneProfile(cmsHPROFILE srcProfile);
}

#endif