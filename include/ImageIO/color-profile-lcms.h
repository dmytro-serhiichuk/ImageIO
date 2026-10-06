#pragma once
#include <lcms2.h>
#include "color-profile.h"
#include <vector>

namespace ImageIO {
    inline ColorProfile colorProfileFromLcms(cmsHPROFILE h) {
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(h, nullptr, &size);
        std::vector<std::uint8_t> buf(size);
        cmsSaveProfileToMem(h, buf.data(), &size);
        return ColorProfile::FromMemory(buf.data(), size);
    }
}