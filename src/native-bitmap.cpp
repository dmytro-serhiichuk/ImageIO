#include "native-bitmap.h"
#include <cstring>
#include <utility>

namespace ImageIO {
    NativeBitmap::NativeBitmap(const NativeBitmap& other) {
        width = other.width;
        height = other.height;
        samplesPerPixel = other.samplesPerPixel;
        bitsPerSample = other.bitsPerSample;
        sampleFormat = other.sampleFormat;
        colorSpace = other.colorSpace;
        
        iccProfileSize = other.iccProfileSize;
        iccProfile = new uint8_t[iccProfileSize]();
        memcpy(iccProfile, other.iccProfile, iccProfileSize);

        dataSize = other.dataSize;
        data = new uint8_t[dataSize]();
        memcpy(data, other.data, dataSize);
    }

    NativeBitmap::NativeBitmap(NativeBitmap&& other) noexcept {
        width = other.width;
        height = other.height;
        samplesPerPixel = other.samplesPerPixel;
        bitsPerSample = other.bitsPerSample;
        sampleFormat = other.sampleFormat;
        colorSpace = other.colorSpace;
        
        iccProfileSize = other.iccProfileSize;
        iccProfile = other.iccProfile;
        other.iccProfile = nullptr;
        other.iccProfileSize = 0;

        dataSize = other.dataSize;
        data = other.data;
        other.data = nullptr;
        other.dataSize = 0;
    }

    NativeBitmap::~NativeBitmap() {         
        delete [] iccProfile;
        iccProfile = nullptr;
        delete [] data;
        data = nullptr;
    }
    NativeBitmap &NativeBitmap::operator=(NativeBitmap &&nb)
    {
        if (this != &nb)
        {
            delete[] iccProfile;
            delete[] data;

            width = nb.width;
            height = nb.height;
            samplesPerPixel = nb.samplesPerPixel;
            bitsPerSample = nb.bitsPerSample;
            sampleFormat = nb.sampleFormat;
            colorSpace = nb.colorSpace;

            iccProfileSize = nb.iccProfileSize;
            iccProfile = nb.iccProfile;

            dataSize = nb.dataSize;
            data = nb.data;

            nb.iccProfile = nullptr;
            nb.data = nullptr;
            nb.iccProfileSize = 0;
            nb.dataSize = 0;
        }

        return *this;
    }
    NativeBitmap &NativeBitmap::operator=(const NativeBitmap &nb)
    {
        if (this != &nb)
        {
            delete[] iccProfile;
            delete[] data;

            width = nb.width;
            height = nb.height;
            samplesPerPixel = nb.samplesPerPixel;
            bitsPerSample = nb.bitsPerSample;
            sampleFormat = nb.sampleFormat;
            colorSpace = nb.colorSpace;

            iccProfileSize = nb.iccProfileSize;
            iccProfile = new uint8_t[iccProfileSize];
            memcpy(iccProfile, nb.iccProfile, iccProfileSize);

            dataSize = nb.dataSize;
            data = new uint8_t[dataSize];
            memcpy(data, nb.data, dataSize);
        }

        return *this;
    }

    Bitmap NativeBitmap::toBitmap(BitmapDepth outDepth, ColorSpace outColorSpace) const 
    {
        if (colorSpace == NativeColorSpace::Unknown) {
            throw std::runtime_error("Converting can not be performed with an unknown color space");
        }

        NativeBitmap temp = *this;
        normalizeDepth(temp, outDepth);

        normalizeColorSpace(temp, outColorSpace);
        
        return Bitmap(1, 1, nullptr, BitmapColorSpace::RGB, BitmapDepth::U8, BitmapColorProfile::sRGB_2_2, nullptr);
    }
    
    // Depth Normalization
    void NativeBitmap::normalizeDepth(NativeBitmap &src, BitmapDepth outDepth) {
        uint8_t targetBytes = Bitmap::getBytesPerSample(outDepth);
        uint8_t targetBits = targetBytes * 8;
        size_t totalSamples = src.width * src.height * src.samplesPerPixel;

        if (src.bitsPerSample == 8 || src.bitsPerSample == 16 || 
            src.bitsPerSample == 32 || src.bitsPerSample == targetBits) 
        {
            src.bitsPerSample = targetBits;
            intToUint(src, totalSamples);
            return;
        }

        size_t outputSize = totalSamples * targetBytes;
        auto outputData = new uint8_t[outputSize];

        for (size_t i = 0; i < totalSamples; i++) {
            uint64_t val = readPackedSample(src, i);
            uint32_t normalized = bitReplicate(val, src.bitsPerSample, targetBits);
            writeSample(outputData, normalized, targetBits, i);
        }

        src.bitsPerSample = targetBits;
        delete src.data;
        src.data = outputData;
        intToUint(src, totalSamples);
    }
    uint64_t NativeBitmap::readPackedSample(NativeBitmap &src, size_t index)
    {
        uint64_t result = 0;
        
        if (src.sampleFormat == SampleFormat::Float) {
            // other values are already discarded in the normalizeDepth method
            if (src.bitsPerSample != 64) {
                throw std::runtime_error("Native Bitmap: In the current library version only Float32 and Float64 are supported");
            }
            memcpy(&result, (uint64_t*)src.data[index], sizeof(uint64_t));
            return result;
        } else {
            if (src.bitsPerSample == 64) {
                return ((uint64_t*)src.data)[index];
            }

            size_t bitOffset = index * src.bitsPerSample;
            uint32_t res32 = 0;

            for (uint8_t i = 0; i < src.bitsPerSample; i++) {
                size_t  byteIdx = (bitOffset + i) / 8;
                uint8_t bitIdx  = 7u - static_cast<uint8_t>((bitOffset + i) % 8); // MSB2LSB
                res32 = (result << 1u) | ((src.data[byteIdx] >> bitIdx) & 1u);
            }
            return static_cast<uint64_t>(res32);
        }
    }
    uint32_t NativeBitmap::bitReplicate(uint64_t val, uint8_t srcBits, uint8_t dstBits)
    {
        if (srcBits == dstBits) return val;
        uint32_t result = 0;
        int remaining = dstBits;
        while (remaining > 0) {
            int take = std::min((int)srcBits, remaining);
            result |= (val >> (srcBits - take)) << (remaining - take);
            remaining -= take;
        }
        return result;
    }
    void NativeBitmap::writeSample(void *outputData, uint32_t value, uint8_t targetBits, size_t index) {
        if (targetBits == 8) {
            memcpy((uint8_t*)outputData + index, &value, sizeof(uint8_t));
        } else if (targetBits == 16) {
            memcpy((uint16_t*)outputData + index, &value, sizeof(uint16_t));
        } else if (targetBits == 32) {
            memcpy((uint32_t*)outputData + index, &value, sizeof(uint32_t));
        } else {
            throw std::runtime_error("Native Bitmap: Invalid target bits per sample value; Supported values: 8/16/32");
        }
    }
    void NativeBitmap::intToUint(NativeBitmap &src, size_t totalSamples) {
        if (src.sampleFormat != SampleFormat::Int) return;

        if (src.bitsPerSample == 8) {
            for (size_t i = 0; i < totalSamples; i++) {
                src.data[i] = (uint8_t)((int16_t)(src.data[i]) + 128);
            }
        } else if (src.bitsPerSample == 16) {
            for (size_t i = 0; i < totalSamples; i++) {
                src.data[i] = (uint16_t)((int32_t)(src.data[i]) + 32768);
            }
        } else {
            for (size_t i = 0; i < totalSamples; i++) {
                src.data[i] = (uint32_t)((int64_t)(src.data[i]) + 2147483648LL);
            }            
        }
    }

    // Color Space Normalization
    void NativeBitmap::normalizeColorSpace(NativeBitmap &src, ColorSpace outColorSpace) {
        uint32_t bytesPerSample = src.bitsPerSample / 8;
        bool isFloat = src.sampleFormat == SampleFormat::Float;

        auto inType = buildLcmsFormatter(src);
        auto outType = outColorSpace.buildLcmsType(bytesPerSample, isFloat);

        auto inProfile = cmsOpenProfileFromMem(src.iccProfile, src.iccProfileSize);
        auto outProfile = outColorSpace.createProfile();

        cmsHTRANSFORM t = cmsCreateTransform(
            inProfile, inType,
            outProfile, outType,
            INTENT_RELATIVE_COLORIMETRIC, 0
        );

        size_t size = src.width * src.height;
        if ((size_t)outColorSpace.channels == src.samplesPerPixel) {
            cmsDoTransform(t, src.data, src.data, size);
        } else {
            size_t newDataSize = size * bytesPerSample * src.samplesPerPixel;        
            auto newData = new uint8_t[newDataSize]();
            cmsDoTransform(t, src.data, newData, size);
            delete [] src.data;
            src.data = newData;
            src.dataSize = newDataSize;
        }
        cmsDeleteTransform(t);
        cmsCloseProfile(inProfile);
        cmsCloseProfile(outProfile);
    }
    cmsUInt32Number NativeBitmap::buildLcmsFormatter(NativeBitmap &src) {
        cmsUInt32Number colorSpaceFlag = 0;
        cmsUInt32Number channelsCount = 0;
        cmsUInt32Number extraChannels = 0;
        cmsUInt32Number bytesPerSample = src.bitsPerSample / 8;
        cmsUInt32Number isFloat = (src.sampleFormat == SampleFormat::Float) ? 1 : 0;

        switch (src.colorSpace)
        {
            case NativeColorSpace::RGB:
            case NativeColorSpace::RGBA:
                colorSpaceFlag = PT_RGB;
                channelsCount = 3;
                extraChannels = src.colorSpace == NativeColorSpace::RGBA ? 1 : 0;
                break;
            case NativeColorSpace::CMYK:
            case NativeColorSpace::CMYKA:
                colorSpaceFlag = PT_CMYK;
                channelsCount = 4;
                extraChannels = src.colorSpace == NativeColorSpace::CMYKA ? 1 : 0;
                break;
            case NativeColorSpace::LAB:
            case NativeColorSpace::ALAB:
                colorSpaceFlag = PT_Lab;
                channelsCount = 3;
                extraChannels = src.colorSpace == NativeColorSpace::ALAB ? 1 : 0;
                break;
            case NativeColorSpace::LAB2:
            case NativeColorSpace::ALAB2:
                colorSpaceFlag = PT_LabV2;
                channelsCount = 3;
                extraChannels = src.colorSpace == NativeColorSpace::ALAB2 ? 1 : 0;
                break;
            default: // gray
                colorSpaceFlag = PT_GRAY;
                channelsCount = 1;
                extraChannels = src.colorSpace == NativeColorSpace::GrayscaleAlpha ? 1 : 0;
                break;
        }
        
        return (COLORSPACE_SH(colorSpaceFlag) |
                CHANNELS_SH(channelsCount) |
                BYTES_SH(bytesPerSample) |
                FLOAT_SH(isFloat) |
                EXTRA_SH(extraChannels));
    }
}