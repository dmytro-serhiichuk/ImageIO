#include "profile-management.h"
#include "cmyk_profile_data.h"
#include <cstdint>
#include <stdexcept>

namespace ImageIO {
    cmsHPROFILE ImageIO::cloneProfile(cmsHPROFILE srcProfile) {
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(srcProfile, nullptr, &size);
        auto buffer = new uint8_t[size];
        cmsSaveProfileToMem(srcProfile, buffer, &size);
        cmsHPROFILE dst = cmsOpenProfileFromMem(buffer, size);
        delete [] buffer;
        return dst;
    }
    void writeProfileToMem(cmsHPROFILE profile, uint8_t *&icc, uint32_t &iccSize) {
        cmsSaveProfileToMem(profile, NULL, &iccSize);
        icc = new uint8_t[iccSize]();
        cmsSaveProfileToMem(profile, icc, &iccSize);
    }
    cmsHPROFILE createDefaultGrayProfile() {
        auto curve = cmsBuildGamma(nullptr, 2.2);
        auto profile = cmsCreateGrayProfile(cmsD50_xyY(), curve);
        cmsFreeToneCurve(curve);
        return profile;
    }
    cmsHPROFILE createCMYKProfile() {
        return cmsOpenProfileFromMem(USWebCoatedSWOP_icc, USWebCoatedSWOP_icc_len);
    }
    cmsUInt32Number buildLcmsType(ColorModel colorModel, SampleType sampleType) {
        cmsUInt32Number colorSpaceFlag = 0;
        cmsUInt32Number channelsCount = 0;
        cmsUInt32Number extraChannels = 0;

        auto channels = getSamplesPerPixel(colorModel);
        auto bps = getBytesPerSample(sampleType);
        auto isFloat = sampleType == SampleType::F32;

        switch (colorModel) {
            case ColorModel::RGB:
            case ColorModel::RGBA:
                colorSpaceFlag = PT_RGB;
                channelsCount = 3;
                extraChannels = channels - 3;
                break;
            case ColorModel::CMYK:
            case ColorModel::CMYKA:
                colorSpaceFlag = PT_CMYK;
                channelsCount = 4;
                extraChannels = channels - 4;
                break;
            case ColorModel::XYZ:
                colorSpaceFlag = PT_XYZ;
                channelsCount = 3;
                extraChannels = 0;
                break;
            default: // gray
                colorSpaceFlag = PT_GRAY;
                channelsCount = 1;
                extraChannels = channels - 1;
                break;
        }
        
        return (COLORSPACE_SH(colorSpaceFlag) |
                CHANNELS_SH(channelsCount) |
                BYTES_SH((cmsUInt32Number)bps) |
                FLOAT_SH((cmsUInt32Number)isFloat) |
                EXTRA_SH(extraChannels));
    }
    cmsHPROFILE createProfileFromColorModel(ColorModel colorModel) {
        switch (colorModel) {
            case ColorModel::RGB:
            case ColorModel::RGBA:
                return cmsCreate_sRGBProfile();
            case ColorModel::GRAY:
            case ColorModel::GRAYA:
                return createDefaultGrayProfile();
            case ColorModel::CMYK:
            case ColorModel::CMYKA:
                return createCMYKProfile();
            case ColorModel::XYZ:
                return cmsCreateXYZProfile();
            default:
                throw std::runtime_error("ICC Profile creation failed - invalid color model");
        }
    }
}
