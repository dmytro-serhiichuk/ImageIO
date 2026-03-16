#ifndef IMAGEIO_NATIVE_BITMAP_H
#define IMAGEIO_NATIVE_BITMAP_H

#include <cstdint>
#include <cstdio>
#include "bitmap.h"
#include "color-space.h"

namespace ImageIO {
    enum class SampleFormat {
        UInt, Int, Float
    };

    enum class NativeColorSpace {
        Unknown, Grayscale, GrayscaleAlpha, RGB, RGBA, CMYK,
        CMYKA, LAB, ALAB, LAB2, ALAB2
    };

    struct NativeBitmap {
        uint32_t width = 0;
        uint32_t height = 0;
        uint8_t samplesPerPixel = 0;
        uint8_t bitsPerSample = 0;

        SampleFormat sampleFormat;
        NativeColorSpace colorSpace;

        uint8_t* iccProfile = nullptr;
        uint32_t iccProfileSize = 0;

        uint8_t* data = nullptr;
        size_t dataSize = 0;

        NativeBitmap() {};
        NativeBitmap(
            uint32_t width, uint32_t height, uint8_t samplesPerPixel, 
            uint8_t bitsPerSample, SampleFormat sampleFormat, 
            NativeColorSpace colorSpace, uint8_t* iccProfile, 
            size_t iccProfileSize, uint8_t* data, size_t dataSize
        ) : width(width), height(height), samplesPerPixel(samplesPerPixel),
            bitsPerSample(bitsPerSample), sampleFormat(sampleFormat), 
            colorSpace(colorSpace), iccProfile(iccProfile), 
            iccProfileSize(iccProfileSize), data(data), dataSize(dataSize) {}
        NativeBitmap(const NativeBitmap& other);
        NativeBitmap(NativeBitmap&& other) noexcept;
        ~NativeBitmap();

        NativeBitmap& operator=(NativeBitmap&& nb);

        NativeBitmap& operator=(const NativeBitmap& nb);

        Bitmap toBitmap(BitmapDepth outDepth, ColorSpace outColorSpace) const;
    
    private:
        // Depth Normalization
        static void normalizeDepth(NativeBitmap &src, BitmapDepth outDepth);
        static uint64_t readPackedSample(NativeBitmap &src, size_t index);
        static uint32_t bitReplicate(uint64_t val, uint8_t srcBits, uint8_t dstBits);
        static void writeSample(void *outputData, uint32_t value, uint8_t targetBits, size_t index);
        static void intToUint(NativeBitmap &src, size_t totalSamples);

        static void normalizeColorSpace(NativeBitmap &src, ColorSpace outColorSpace);
        static cmsUInt32Number buildLcmsFormatter(NativeBitmap &src);
    };
}

#endif