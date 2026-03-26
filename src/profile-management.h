#ifndef IMAGEIO_PROFILE_MANAGEMENT_H
#define IMAGEIO_PROFILE_MANAGEMENT_H

#include <lcms2.h>
#include <cstdint>
#include "color-model.h"
#include "sample-type.h"

namespace ImageIO {
    cmsHPROFILE cloneProfile(cmsHPROFILE srcProfile);
    void writeProfileToMem(cmsHPROFILE profile, uint8_t*& icc, uint32_t &iccSize);
    cmsHPROFILE createDefaultGrayProfile();
    cmsHPROFILE createCMYKProfile();

    cmsUInt32Number buildLcmsType(ColorModel colorModel, SampleType sampleType);
    cmsHPROFILE createProfileFromColorModel(ColorModel colorModel);
}

#endif