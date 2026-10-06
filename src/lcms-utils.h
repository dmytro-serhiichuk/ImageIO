#pragma once

#include <lcms2.h>
#include "ImageIO/color-model.h"
#include "ImageIO/sample-type.h"
#include "ImageIO/color-profile.h"

namespace ImageIO {
    cmsUInt32Number buildLcmsType(ColorModel, SampleType);
    bool profileEqual(cmsHPROFILE p1, cmsHPROFILE p2);

    inline cmsHPROFILE getNativeLcmsProfile(const ColorProfile& p) { 
        return static_cast<cmsHPROFILE>(p.nativeHandle()); 
    }
}