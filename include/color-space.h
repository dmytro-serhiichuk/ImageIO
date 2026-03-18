#ifndef IMAGEIO_COLOR_SPACE_H
#define IMAGEIO_COLOR_SPACE_H

#include <lcms2.h>
#include <cstdint>

namespace ImageIO {
    enum class Channels {
        RGB = 3,
        RGBA = 4,
        /*
        * Represents data with 1 sample per pixel. Always works with 
        * ColorGamut::Grayscale, other gamut values are ignored
        */ 
        Grayscale = 1,
        /*
        * Represents data with 2 samples per pixel where second sample represents 
        * alpha value. Always works with ColorGamut::Grayscale, other gamut 
        * values are ignored
        */ 
        GrayscaleAlpha = 2,
    };

    enum class ColorGamut {
        // Represents gamut according to IEC 61966-2-1 standart. Uses D65 white point
        sRGB,
        // Represents gamut according to ISO 22028-2 standart. Uses D50 white point
        ProPhoto,
        // Represents gamut according to Adobe RGB (1998) standart. Uses D65 white point
        AdobeRGB,
        // Represents gamut according to ISO 15076-1 standart. Uses D50 white point
        Grayscale,
    };

    enum class TransferFunction {
        // A linear gamma reresents the 1.0 value
        Linear,
        // Default gamma used by sRGB gamut. Uses IEC 61966-2-1 standart
        sRGB,
        // Default gamma for ProPhoto gamut
        Gamma_1_8,
        // Default gamma for AdobeRGB gamut
        Gamma_2_2
    };

    struct ColorSpace {
        Channels channels;
        ColorGamut gamut;
        TransferFunction transfer;

        // Represents RGB data in sRGB color profile
        static ColorSpace sRGB() {
            return { Channels::RGB, ColorGamut::sRGB, TransferFunction::sRGB };
        }
        // Represents RGBA data in sRGB color profile
        static ColorSpace sRGBA() {
            return { Channels::RGBA, ColorGamut::sRGB, TransferFunction::sRGB };
        }

        // Represents RGB data in sRGB color profile with linear (1.0) gamma
        static ColorSpace sRGB_Linear() {
            return { Channels::RGB, ColorGamut::sRGB, TransferFunction::Linear };
        }
        // Represents RGBA data in sRGB color profile with linear (1.0) gamma
        static ColorSpace sRGBA_Linear() {
            return { Channels::RGBA, ColorGamut::sRGB, TransferFunction::Linear };
        }

        // Represents RGB data in AdobeRGB color profile
        static ColorSpace AdobeRGB() {
            return { Channels::RGB, ColorGamut::AdobeRGB, TransferFunction::Gamma_2_2 };
        }
        // Represents RGB data in AdobeRGBA color profile
        static ColorSpace AdobeRGBA() {
            return { Channels::RGBA, ColorGamut::AdobeRGB, TransferFunction::Gamma_2_2 };
        }

        // Represents RGB data in ProPhoto color profile
        static ColorSpace ProPhoto() {
            return { Channels::RGB, ColorGamut::ProPhoto, TransferFunction::Gamma_1_8 };
        }
        // Represents RGBA data in ProPhoto color profile
        static ColorSpace ProPhotoAlpha() {
            return { Channels::RGBA, ColorGamut::ProPhoto, TransferFunction::Gamma_1_8 };
        }

        // Represents Grayscale data
        static ColorSpace Grayscale() {
            return { Channels::Grayscale, ColorGamut::Grayscale, TransferFunction::Linear };
        }
        // Represents Grayscale data with alpha channel
        static ColorSpace GrayscaleAlpha() {
            return { Channels::GrayscaleAlpha, ColorGamut::Grayscale, TransferFunction::Linear };
        }

        bool operator==(const ColorSpace& other) const;

        // Checks if the current color space configuration is valid
        bool isValid() const;
        /* 
         * Checks if the transofrmation from current color space to the new one 
         * requires the profile reconstruction.
         *
         * There are different reasons why the profile needs to be recreated, it
         * may be the difference in gamuts or gamma levels even if the channels
         * formats are the same, but it also can be caused by transformation 
         * between formats that cannot have the same gamut like RGB and Grayscale  
        */
        bool isRequireProfileReconstruction(const ColorSpace &other) const;
        bool isHaveSameChannelsNumber(const ColorSpace &other) const;

        /// @brief Builds lcms2 format type for transformations
        /// @param bytesPerSample precesion of image data 
        /// @param isFloat tells the method whether the image data represented 
        /// by float data type or not
        /// @return lcms2 format type
        cmsUInt32Number buildLcmsType(uint32_t bytesPerSample, bool isFloat) const;
        /// @brief Creates a color profile depends on the gamut and gamma
        /// @return New instance of cmsHPROFILE that can be used by lcms2 library
        cmsHPROFILE createProfile() const;
    };
}

#endif