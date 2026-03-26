#ifndef IMAGEIO_BITMAP_H
#define IMAGEIO_BITMAP_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "properties.h"
#include <lcms2.h>
#include <vector>
#include "color-model.h"
#include "sample-type.h"

namespace ImageIO {
    class Bitmap {
    public:
        uint32_t width;
        uint32_t height;
        uint8_t* buffer;
        size_t bufferSize;
        size_t totalSamples;
        uint32_t stride;
        SampleType sampleType;
        ColorModel colorModel;
        cmsHPROFILE profile;

        Bitmap();
        // New instance of the Bitmap takes ownership over passed buffer and icc profile
        Bitmap(uint32_t w, uint32_t h, void* b, SampleType st, ColorModel cm, cmsHPROFILE p) :
            width(w), height(h), buffer((uint8_t*)b), sampleType(st), 
            colorModel(cm), profile(p),
            totalSamples(w * h * getSamplesPerPixel(cm)),
            bufferSize(w * h * getSamplesPerPixel(cm) * getBytesPerSample(st)),
            stride(w * getBytesPerSample(st) * getSamplesPerPixel(cm)) {}
        Bitmap(const Bitmap& other);
        Bitmap(Bitmap&& other) noexcept;
        ~Bitmap();

        Bitmap copy() const;

        Bitmap& operator=(Bitmap&& nb);
        Bitmap& operator=(const Bitmap& nb);

        /// @brief Converts bitmap's sample type
        /// @return New instance of a bitmap with converted sample type
        /// @note Returns a copy of the bitmap if newSampleType is equal to bitmap's sample type
        inline Bitmap convertSampleType(const SampleType newSampleType, Properties props = {}) const {
            return convertTo(newSampleType, colorModel, nullptr, props);
        }

        /// @brief Converts bitmap's color model
        /// @return New instance of a bitmap with converted color model and new color profile
        /// @note Returns a copy of the bitmap if newColorModel is equal to bitmap's color model
        /// @note Always recreates the bitmap if newProfile is explicitly defined
        /// @note The resulted bitmap takes ownership over the passed icc profile
        inline Bitmap convertColorModel(const ColorModel newColorModel, const cmsHPROFILE newProfile = nullptr, Properties props = {}) const {
            return convertTo(sampleType, newColorModel, newProfile, props);
        }

        /// @brief Converts bitmap's sample type and color model
        /// @return New instance of bitmap with converted values
        /// @note Returns a copy of the bitmap if newSampleType and newColorModel are equal to current bitmap's values
        /// @note Always recreates the bitmap if newProfile is explicitly defined
        /// @note The resulted bitmap takes ownership over the passed icc profile
        Bitmap convertTo(const SampleType newSampleType, const ColorModel newColorModel, const cmsHPROFILE newProfile = nullptr, Properties props = {}) const;

        /// @brief Returns ICC Profile of the bitmap
        /// @return ICC Profile stored in memory
        std::vector<uint8_t> getICCProfile() const;
    private:
        Bitmap convertByLcms2(SampleType newSampleType, ColorModel newColorSpace, const cmsHPROFILE newProfile = nullptr) const;

        template <typename I, typename O>
        O* _convertTo(SampleType newSampleType, ColorModel newColorModel, Properties props) const {
            size_t newBufferLength = width * height * getSamplesPerPixel(newColorModel); 
            O* dst = new O[newBufferLength];
            I* src = (I*)buffer;

            if (colorModel == newColorModel) {
                for (size_t i = 0; i < totalSamples; i++) {
                    dst[i] = Bitmap::convertSampleValue<I, O>(src[i]);
                }
            } else if (newColorModel == ColorModel::RGB) /* to RGB */ {
                if (colorModel == ColorModel::RGBA) /* from RGBA */ {
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=4, newI+=3) {
                        dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
                        dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
                    }
                }
            } else if (newColorModel == ColorModel::GRAY) /* to Grayscale */ {
                if (colorModel == ColorModel::GRAYA) /* from Grayscale Alpha */ {
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=2, newI+=1) {
                        dst[newI] = Bitmap::convertSampleValue<I, O>(src[i]);
                    }
                }
            } else if (newColorModel == ColorModel::CMYK) /* to CMYK */ {
                if (colorModel == ColorModel::CMYKA) /* to CMYKA */ {
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=5, newI+=4) {
                        dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
                        dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
                        dst[newI + 3] = Bitmap::convertSampleValue<I, O>(src[i + 3]);
                    }
                }
            } else if (newColorModel == ColorModel::RGBA) { /* to RGBA */
                if (colorModel == ColorModel::RGB) { /* from RGB */
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=3, newI+=4) {
                        dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
                        dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
                        dst[newI + 3] = Bitmap::convertSampleValue<uint8_t, O>(255);
                    }
                }
            } else if (newColorModel == ColorModel::GRAYA) { /* to GrayA */
                if (colorModel == ColorModel::GRAY) { /* from Gray */
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=1, newI+=2) {
                        dst[newI] = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<uint8_t, O>(255);
                    }
                }
            } else if (newColorModel == ColorModel::CMYKA) { /* to CMYKA */
                if (colorModel == ColorModel::CMYK) { /* from CMYK */
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=4, newI+=5) {
                        dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
                        dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
                        dst[newI + 3] = Bitmap::convertSampleValue<I, O>(src[i + 3]);
                        dst[newI + 4] = Bitmap::convertSampleValue<uint8_t, O>(255);
                    }
                }
            }

            return dst;
        }
        
        template <typename I, typename O>
        static O convertSampleValue(I input) {
            if (std::is_same<I, O>::value) return input;

            if (std::is_same<I, uint8_t>::value) {
                if (std::is_same<O, uint16_t>::value)   return (uint16_t)input * 257;
                if (std::is_same<O, uint32_t>::value)   return (uint32_t)input * 16843009;
                if (std::is_same<O, float>::value)      return (float)input / 255;
            }
            if (std::is_same<I, uint16_t>::value) {
                if (std::is_same<O, uint8_t>::value)    return (uint8_t)((uint16_t)input >> 8);
                if (std::is_same<O, uint32_t>::value)   return (uint32_t)input * 65537;
                if (std::is_same<O, float>::value)      return (float)input / 65535;
            }
            if (std::is_same<I, uint32_t>::value) {
                if (std::is_same<O, uint8_t>::value)    return (uint8_t)((uint32_t)input >> 24);
                if (std::is_same<O, uint16_t>::value)   return (uint16_t)((uint32_t)input >> 16);
                if (std::is_same<O, float>::value)      return (float)input / 4294967295;
            }
            if (std::is_same<I, float>::value) {
                if (std::is_same<O, uint8_t>::value)    return (O)(input * 255);
                if (std::is_same<O, uint16_t>::value)   return (O)(input * 65535);
                if (std::is_same<O, uint32_t>::value)   return (O)(input * 4294967295);
            }

            throw std::invalid_argument("Unsupported types. Bitmap only supports U8, U16, U32 and F32 types");
        }
    };
}

#endif //IMAGEIO_BITMAP_H