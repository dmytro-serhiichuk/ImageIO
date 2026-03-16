#ifndef IMAGEIO_COLOR_SPACE_H
#define IMAGEIO_COLOR_SPACE_H

#include <lcms2.h>

namespace ImageIO {
    enum class Channels {
        RGB = 3,
        RGBA = 4,
        Grayscale = 1,
        GrayscaleAlpha = 2,
    };

    enum class ColorGamut {
        sRGB,
        ProPhoto,
        AdobeRGB,
        XYZ,
        Grayscale,
    };

    enum class TransferFunction {
        Linear,
        sRGB,
        Gamma_1_8,
    };

    struct ColorSpace {
        Channels channels;
        ColorGamut gamut;
        TransferFunction transfer;

        static ColorSpace sRGB() {
            return { Channels::RGB, ColorGamut::sRGB, TransferFunction::sRGB };
        }
        static ColorSpace sRGBA() {
            return { Channels::RGBA, ColorGamut::sRGB, TransferFunction::sRGB };
        }

        static ColorSpace sRGB_Linear() {
            return { Channels::RGB, ColorGamut::sRGB, TransferFunction::Linear };
        }
        static ColorSpace sRGBA_Linear() {
            return { Channels::RGBA, ColorGamut::sRGB, TransferFunction::Linear };
        }

        static ColorSpace ProPhoto() {
            return { Channels::RGB, ColorGamut::ProPhoto, TransferFunction::Gamma_1_8 };
        }
        static ColorSpace ProPhotoAlpha() {
            return { Channels::RGBA, ColorGamut::ProPhoto, TransferFunction::Gamma_1_8 };
        }

        static ColorSpace XYZ() {
            return { Channels::RGB, ColorGamut::XYZ, TransferFunction::Linear };
        }

        static ColorSpace Grayscale() {
            return { Channels::Grayscale, ColorGamut::Grayscale, TransferFunction::Linear };
        }
        static ColorSpace GrayscaleAlpha() {
            return { Channels::GrayscaleAlpha, ColorGamut::Grayscale, TransferFunction::Linear };
        }

        bool operator==(const ColorSpace& other) const {
            return channels == other.channels
                && gamut == other.gamut
                && transfer == other.transfer;
        }

        bool isValid() const {
            bool isGray = (channels == Channels::Grayscale || channels == Channels::GrayscaleAlpha);
            bool gamutIsGray = (gamut == ColorGamut::Grayscale);
            return isGray == gamutIsGray;
        }

        cmsUInt32Number buildLcmsType(uint32_t bytesPerSample, bool isFloat) {
            cmsUInt32Number colorSpaceFlag = 0;
            cmsUInt32Number channelsCount = 0;
            cmsUInt32Number extraChannels = 0;

            switch (channels)
            {
                case Channels::RGB:
                case Channels::RGBA:
                    colorSpaceFlag = PT_RGB;
                    channelsCount = 3;
                    extraChannels = channels == Channels::RGBA ? 1 : 0;
                    break;
                default: // gray
                    colorSpaceFlag = PT_GRAY;
                    channelsCount = 1;
                    extraChannels = channels == Channels::GrayscaleAlpha ? 1 : 0;
                    break;
            }
            
            return (COLORSPACE_SH(colorSpaceFlag) |
                    CHANNELS_SH(channelsCount) |
                    BYTES_SH((cmsUInt32Number)bytesPerSample) |
                    FLOAT_SH((cmsUInt32Number)isFloat) |
                    EXTRA_SH(extraChannels));
        }

        cmsHPROFILE createProfile() {
            cmsToneCurve* curve = nullptr;
            switch (transfer) {
                case TransferFunction::Linear:
                    curve = cmsBuildGamma(nullptr, 1.0);
                    break;
                case TransferFunction::sRGB: {
                    cmsFloat64Number p[7] = {
                        2.4,              // γ
                        1.0 / 1.055,      // a
                        0.055 / 1.055,    // b
                        0.0,              // c
                        0.04045,          // d
                        1.0 / 12.92,      // e
                        0.0               // f
                    };
                    curve = cmsBuildParametricToneCurve(nullptr, 4, p);
                    break;
                }
                case TransferFunction::Gamma_1_8:
                    curve = cmsBuildGamma(nullptr, 1.8);
                    break;
            }

            if (gamut == ColorGamut::XYZ) {
                cmsFreeToneCurve(curve);
                return cmsCreateXYZProfile();
            }

            if (gamut == ColorGamut::Grayscale) {
                cmsHPROFILE h = cmsCreateGrayProfile(cmsD50_xyY(), curve);
                cmsFreeToneCurve(curve);
                return h;
            }
            
            cmsCIExyY wp;
            cmsCIExyYTRIPLE primaries;
            switch (gamut) {
                case ColorGamut::sRGB:
                    // IEC 61966-2-1
                    primaries = {
                        { 0.6400, 0.3300, 1.0 },
                        { 0.3000, 0.6000, 1.0 },
                        { 0.1500, 0.0600, 1.0 }
                    };
                    // D65
                    wp = { 0.3127, 0.3290, 1.0 };
                    break;

                case ColorGamut::AdobeRGB:
                    // Adobe RGB (1998)
                    primaries = {
                        { 0.6400, 0.3300, 1.0 },
                        { 0.2100, 0.7100, 1.0 },
                        { 0.1500, 0.0600, 1.0 }
                    };
                    // D65
                    wp = { 0.3127, 0.3290, 1.0 };
                    break;

                case ColorGamut::ProPhoto:
                    // ROMM RGB / ISO 22028-2
                    primaries = {
                        { 0.7347, 0.2653, 1.0 },
                        { 0.1596, 0.8404, 1.0 },
                        { 0.0366, 0.0001, 1.0 }
                    };
                    // D50
                    wp = { 0.3457, 0.3585, 1.0 };
                    break;

                default:
                    cmsFreeToneCurve(curve);
                    return nullptr;
            }

            cmsToneCurve* curves[3] = { curve, curve, curve };
            cmsHPROFILE h = cmsCreateRGBProfile(&wp, &primaries, curves);
            cmsFreeToneCurve(curve);
            return h;
        }
    };
}

#endif