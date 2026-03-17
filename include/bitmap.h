#ifndef IMAGEIO_BITMAP_H
#define IMAGEIO_BITMAP_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "properties.h"
#include <lcms2.h>
#include <vector>
#include "color-space.h"

namespace ImageIO {
    enum class SampleType {
        U8,
        U16,
        U32,
        F32
    };
    
    class Bitmap {
    private:
        uint8_t* buffer;
        cmsHPROFILE profile;
    public:
        uint32_t width;
        uint32_t height;
        size_t totalSamples;
        uint32_t stride;
        uint8_t samplesPerPixel;
        SampleType sampleType;
        ColorSpace colorSpace;

        Bitmap(uint32_t w, uint32_t h, void* b, SampleType st, ColorSpace cs, cmsHPROFILE p) : 
            width(w), height(h), buffer((uint8_t*)b), colorSpace(cs), sampleType(st), profile(p), samplesPerPixel((uint8_t)cs.channels)
        {
            totalSamples = width * height * samplesPerPixel;
            stride = width * samplesPerPixel;
        }
        Bitmap(const Bitmap& other);
        Bitmap(Bitmap&& other) noexcept;
        ~Bitmap();

        Bitmap copy() const;

        template <typename T>
        T& operator[](size_t index) {
            if (index >= bufferLength) throw std::out_of_range("Index of range");            
            return reinterpret_cast<T*>(buffer)[index];
        }

        template <typename T>
        const T& operator[](size_t index) const {
            if (index >= bufferLength) throw std::out_of_range("Index of range");            
            return reinterpret_cast<T*>(buffer)[index];
        }

        Bitmap& operator=(Bitmap&& nb);

        Bitmap& operator=(const Bitmap& nb);

        /// @brief Returns pointer to bitmap's buffer        
        template <typename T>
        inline T* ptr() {
            return reinterpret_cast<T*>(buffer);
        }

        /// @brief Returns pointer to bitmap's buffer        
        template <typename T>
        inline T* ptr() const {
            return reinterpret_cast<T*>(buffer);
        }

        /// @brief Returns size of the buffer in bytes        
        inline size_t sizeOfBuffer() const;

        /// @brief Converts bitmap's sample type
        /// @return New instance of a bitmap with converted sample type
        /// @note Returns a copy of the bitmap if newSampleType is equal to bitmap's sample type
        Bitmap convertSampleType(const SampleType newSampleType, Properties props = {}) const;

        /// @brief Converts bitmap's color space
        /// @return New instance of a bitmap with converted color space
        /// @note Returns a copy of the bitmap if newColorSpace is equal to bitmap's color space
        Bitmap convertColorSpace(const ColorSpace newColorSpace, Properties props = {}) const;

        /// @brief Converts bitmap's colors         
        /// @return New instance of bitmap with converted colors        
        /// @note Returns copy of the bitmap if newColorSpace is equal to this.colorSpace
        // inline Bitmap* convertColor(BitmapColorSpace newColorSpace, Properties props = {}) const
        // {
        //     return Bitmap::convertTo(depth, newColorSpace, props);
        // }
        
        /// @brief Converts bitmap's depth and color
        /// @return New instance of bitmap with converted values
        /// @note Returns copy of the bitmap if newDepth and newColorSpace are equal to this.depth and this.colorSpace
        // Bitmap* convertTo(SampleType newDepth, BitmapColorSpace newColorSpace, Properties props = {}) const;

        /// @brief Returns size of buffer element depends on bitmap's depth        
        static size_t getBytesPerSample(const SampleType sampleType);

        /// @brief Returns ICC Profile of the bitmap
        /// @return ICC Profile stored in memory
        std::vector<uint8_t> getICCProfile() const;
    private:

        template <typename I, typename O>
        O* _convertSampleType(SampleType newSampleType, Properties props) const {
            auto dst = new O[totalSamples];
            auto src = ptr<I>();

            for (size_t i = 0; i < totalSamples; i++) {
                dst[i] = convertSampleValue<I, O>(src[i]);
            }
            return dst;
        }
        

        // template <typename I, typename O>
        // Bitmap* convert(SampleType newDepth, BitmapColorSpace newColorSpace, Properties props) const {    
        //     size_t newBufferLength = width * height * (size_t)newColorSpace; 
        //     O* dst = new O[newBufferLength];
        //     I* src = ptr<I>();

        //     if (colorSpace == newColorSpace) {
        //         for (size_t i = 0; i < bufferLength; i++) {
        //             dst[i] = Bitmap::convertSampleValue<I, O>(src[i]);
        //         }
        //     }
        //     else if (newColorSpace == BitmapColorSpace::RGB) /* to RGB */ {
        //         if (colorSpace == BitmapColorSpace::Grayscale) /* from Grayscale */ {
        //             for (size_t i = 0; i < bufferLength; i++) {
        //                 dst[i * 3]     = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[i * 3 + 1] = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[i * 3 + 2] = Bitmap::convertSampleValue<I, O>(src[i]);
        //             }
        //         }
        //         else if (colorSpace == BitmapColorSpace::RGBA) /* from RGBA */ {
        //             for (size_t i = 0, newI = 0; i < bufferLength; i+=4, newI+=3) {
        //                 dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
        //                 dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
        //             }
        //         }
        //     }
        //     else if (newColorSpace == BitmapColorSpace::Grayscale) /* to Grayscale */ {
        //         if (colorSpace == BitmapColorSpace::RGB) /* from RGB */ {
        //             for (size_t i = 0; i < newBufferLength; i++) {
        //                 I gray = static_cast<I>(0.299 * src[i * 3] + 0.587 * src[i * 3 + 1] + 0.114 * src[i * 3 + 2]);
        //                 dst[i] = Bitmap::convertSampleValue<I, O>(gray);
        //             }
        //         }
        //         else if (colorSpace == BitmapColorSpace::RGBA) /* from RGBA */ {
        //             for (size_t i = 0; i < newBufferLength; i++) {
        //                 I gray = static_cast<I>(0.299 * src[i * 4] + 0.587 * src[i * 4 + 1] + 0.114 * src[i * 4 + 2]);
        //                 dst[i] = Bitmap::convertSampleValue<I, O>(gray);
        //             }
        //         }
        //     }
        //     else if (newColorSpace == BitmapColorSpace::RGBA) /* to RGBA */ {
        //         if (colorSpace == BitmapColorSpace::RGB) /* from RGB */ {
        //             for (size_t i = 0, newI = 0; i < bufferLength; i+=3, newI+=4) {
        //                 dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
        //                 dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
        //                 dst[newI + 3] = Bitmap::convertSampleValue<uint8_t, O>(255);
        //             }
        //         }
        //         else if (colorSpace == BitmapColorSpace::Grayscale) /* from Grayscale */ {
        //             for (size_t i = 0; i < bufferLength; i++) {
        //                 dst[i * 4]     = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[i * 4 + 1] = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[i * 4 + 2] = Bitmap::convertSampleValue<I, O>(src[i]);
        //                 dst[i * 4 + 3] = Bitmap::convertSampleValue<uint8_t, O>(255);
        //             }
        //         }
        //     }

        //     return new Bitmap(width, height, dst, newColorSpace, newDepth, colorProfile, _profile);
        // }

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