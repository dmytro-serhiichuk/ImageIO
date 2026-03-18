#ifndef IMAGEIO_COLOR_SPACE_H
#define IMAGEIO_COLOR_SPACE_H

#include <lcms2.h>
#include <cstdint>

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

        bool operator==(const ColorSpace& other) const;

        bool isValid() const;
        bool isRequireProfileReconstruction(const ColorSpace &other) const;
        bool isHaveSameChannelsNumber(const ColorSpace &other) const;

        cmsUInt32Number buildLcmsType(uint32_t bytesPerSample, bool isFloat) const;
        cmsHPROFILE createProfile() const;
    };
}

#endif