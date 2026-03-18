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
            stride = width * samplesPerPixel * getBytesPerSample(sampleType);
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
        inline size_t sizeOfBuffer() const {
            return totalSamples * Bitmap::getBytesPerSample(sampleType);
        }

        /// @brief Converts bitmap's sample type
        /// @return New instance of a bitmap with converted sample type
        /// @note Returns a copy of the bitmap if newSampleType is equal to bitmap's sample type
        inline Bitmap convertSampleType(const SampleType newSampleType, Properties props = {}) const {
            return convertTo(newSampleType, colorSpace, props);
        }

        /// @brief Converts bitmap's color space
        /// @return New instance of a bitmap with converted color space
        /// @note Returns a copy of the bitmap if newColorSpace is equal to bitmap's color space
        inline Bitmap convertColorSpace(const ColorSpace newColorSpace, Properties props = {}) const {
            return convertTo(sampleType, newColorSpace, props);
        }

        /// @brief Converts bitmap's sample type and color space
        /// @return New instance of bitmap with converted values
        /// @note Returns copy of the bitmap if newSampleType and newColorSpace are equal to current bitmap's values
        Bitmap convertTo(const SampleType newSampleType, const ColorSpace newColorSpace, Properties props = {}) const;
        
        /// @brief Returns size of buffer element depends on bitmap's depth        
        static size_t getBytesPerSample(const SampleType sampleType);

        /// @brief Returns ICC Profile of the bitmap
        /// @return ICC Profile stored in memory
        std::vector<uint8_t> getICCProfile() const;
    private:
        Bitmap convertByLcms2(SampleType newSampleType, ColorSpace newColorSpace) const;

        template <typename I, typename O>
        O* _convertTo(SampleType newSampleType, ColorSpace newColorSpace, Properties props) const {
            size_t newBufferLength = width * height * (size_t)newColorSpace.channels; 
            O* dst = new O[newBufferLength];
            I* src = ptr<I>();

            Channels inChannels  = colorSpace.channels;
            Channels outChannels = newColorSpace.channels;

            if (inChannels == outChannels) {
                for (size_t i = 0; i < totalSamples; i++) {
                    dst[i] = Bitmap::convertSampleValue<I, O>(src[i]);
                }
            } else if (outChannels == Channels::RGB) /* to RGB */ {
                if (inChannels == Channels::RGBA) /* from RGBA */ {
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=4, newI+=3) {
                        dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
                        dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
                    }
                }
            } else if (outChannels == Channels::Grayscale) /* to Grayscale */ {
                if (inChannels == Channels::GrayscaleAlpha) /* from Grayscale Alpha */ {
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=2, newI+=1) {
                        dst[newI] = Bitmap::convertSampleValue<I, O>(src[i]);
                    }
                }
            } else if (outChannels == Channels::RGBA) { /* to RGBA */
                if (inChannels == Channels::RGB) { /* from RGB */
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=3, newI+=4) {
                        dst[newI]     = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<I, O>(src[i + 1]);
                        dst[newI + 2] = Bitmap::convertSampleValue<I, O>(src[i + 2]);
                        dst[newI + 3] = Bitmap::convertSampleValue<uint8_t, O>(255);
                    }
                }
            } else if (outChannels == Channels::GrayscaleAlpha) { /* to GrayA */
                if (inChannels == Channels::Grayscale) { /* from Gray */
                    for (size_t i = 0, newI = 0; i < totalSamples; i+=1, newI+=2) {
                        dst[newI] = Bitmap::convertSampleValue<I, O>(src[i]);
                        dst[newI + 1] = Bitmap::convertSampleValue<uint8_t, O>(255);
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