#ifndef IMAGEIO_PROFILE_MANAGEMENT_H
#define IMAGEIO_PROFILE_MANAGEMENT_H

#include <lcms2.h>
#include <cstdint>
#include "color-model.h"
#include "sample-type.h"

namespace ImageIO {
    cmsHPROFILE cloneProfile(cmsHPROFILE srcProfile);
    void writeProfileToMem(cmsHPROFILE profile, uint8_t*& icc, uint32_t &iccSize);
    // Creates an icc profile for grayscale color model with D50 white point and linear gamma
    cmsHPROFILE createDefaultGrayProfile();
    // Creates an icc profile for CMYK color model based on U.S. Web Coated (SWOP) v2 standart
    cmsHPROFILE createCMYKProfile();

    // Builds a lcms type for profiles convertations based on color model and sample type
    cmsUInt32Number buildLcmsType(ColorModel colorModel, SampleType sampleType);
    cmsHPROFILE createProfileFromColorModel(ColorModel colorModel);
}

#endif