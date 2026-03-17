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
}