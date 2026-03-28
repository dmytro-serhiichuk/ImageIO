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
    // Creates an icc profile for CMYK color model based on U.S. Web Coated (SWOP) v2 standard
    cmsHPROFILE createCMYKProfile();
    // Creates an icc profile for RGB color model based on Adobe RGB (1998) standard
    cmsHPROFILE createAdobeRGBProfile();
    // Creates an icc profile for RGB color model based on ISO 22028-2:2013 standard
    cmsHPROFILE createProPhotoProfile();
    // Creates an icc profile for RGB color model based on Adobe Wide-Gamut RGB standard
    cmsHPROFILE createWideGamutProfile();

    // Builds a lcms type for profiles convertations based on color model and sample type
    cmsUInt32Number buildLcmsType(ColorModel colorModel, SampleType sampleType);
    // Creates a default profile for passed color model
    cmsHPROFILE createProfileFromColorModel(ColorModel colorModel);
}

#endif