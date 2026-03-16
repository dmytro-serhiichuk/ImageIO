#include "profile-management.h"

namespace ImageIO {
    cmsHPROFILE cloneProfile(cmsHPROFILE srcProfile) {
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(srcProfile, nullptr, &size);
        void* buffer = malloc(size);
        cmsSaveProfileToMem(srcProfile, buffer, &size);
        cmsHPROFILE dst = cmsOpenProfileFromMem(buffer, size);
        free(buffer);
        return dst;
    }

    cmsHPROFILE getCmsHPROFILE(BitmapColorProfile colorProfile) {
        if (colorProfile == BitmapColorProfile::sRGB_2_2) {
            return cmsCreate_sRGBProfile();
        }

        throw std::runtime_error("Unsupported color profile");
    }
}