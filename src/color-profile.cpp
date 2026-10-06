#include "ImageIO/color-profile.h"
#include <lcms2.h>
#include <stdexcept>
#include "profiles-sources/adobe_rgb_profile_data.h"
#include "profiles-sources/cmyk_profile_data.h"
#include "profiles-sources/pro_photo_profile_data.h"

namespace ImageIO {
    struct ColorProfile::Impl {
        explicit Impl(cmsHPROFILE h) : handle(h) {}

        ~Impl() { cmsCloseProfile(handle); }
        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;
        cmsHPROFILE handle;
    };

    ColorProfile ColorProfile::adopt(void* h) {
        if (!h) throw std::runtime_error("ICC profile creation failed");
        ColorProfile p;
        p.impl_ = std::make_shared<Impl>(static_cast<cmsHPROFILE>(h));
        return p;
    }

    ColorProfile ColorProfile::sRGB()       { return adopt(cmsCreate_sRGBProfile()); }
    ColorProfile ColorProfile::AdobeRGB()   { return adopt(cmsOpenProfileFromMem(AdobeRGB1998_icc, AdobeRGB1998_icc_len)); }
    ColorProfile ColorProfile::ProPhotoRGB(){ return adopt(cmsOpenProfileFromMem(ISO22028_2_ROMM_RGB_icc, ISO22028_2_ROMM_RGB_icc_len)); }
    ColorProfile ColorProfile::CMYK()       { return adopt(cmsOpenProfileFromMem(USWebCoatedSWOP_icc, USWebCoatedSWOP_icc_len)); }
    ColorProfile ColorProfile::XYZ()        { return adopt(cmsCreateXYZProfile()); }

    ColorProfile ColorProfile::Gray() {
        cmsToneCurve* curve = cmsBuildGamma(nullptr, 2.2);
        cmsHPROFILE h = cmsCreateGrayProfile(cmsD50_xyY(), curve);
        cmsFreeToneCurve(curve);
        return adopt(h);
    }

    ColorProfile ColorProfile::FromMemory(const void* icc, std::size_t size) {
        return adopt(cmsOpenProfileFromMem(icc, static_cast<cmsUInt32Number>(size)));
    }
    ColorProfile ColorProfile::FromFile(const char* path) {
        return adopt(cmsOpenProfileFromFile(path, "r"));
    }

    ColorProfile ColorProfile::Default(ColorModel m) {
        switch (m) {
            case ColorModel::RGB:  case ColorModel::RGBA:  return sRGB();
            case ColorModel::GRAY: case ColorModel::GRAYA: return Gray();
            case ColorModel::CMYK: case ColorModel::CMYKA: return CMYK();
            case ColorModel::XYZ:                          return XYZ();
            default: throw std::runtime_error("ICC Profile creation failed - invalid color model");
        }
    }

    std::vector<std::uint8_t> ColorProfile::toICC() const {
        std::vector<std::uint8_t> out;
        if (!impl_) return out;
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(impl_->handle, nullptr, &size);
        out.resize(size);
        cmsSaveProfileToMem(impl_->handle, out.data(), &size);
        return out;
    }

    void* ColorProfile::nativeHandle() const noexcept { 
        return impl_ ? impl_->handle : nullptr; 
    }
}