#include "bitmap.h"
#include "profile-management.h"

namespace ImageIO {
    Bitmap::Bitmap(const Bitmap& other) {
        width = other.width;
        height = other.height;
        totalSamples = other.totalSamples;
        stride = other.stride;
        samplesPerPixel = other.samplesPerPixel;
        sampleType = other.sampleType;
        colorSpace = other.colorSpace;

        size_t size = other.sizeOfBuffer();
        buffer = new uint8_t[size];
        memcpy(buffer, other.buffer, size);

        profile = cloneProfile(other.profile);
    }

    Bitmap::Bitmap(Bitmap&& other) noexcept {
        width = other.width;
        height = other.height;
        totalSamples = other.totalSamples;
        stride = other.stride;
        samplesPerPixel = other.samplesPerPixel;
        sampleType = other.sampleType;
        colorSpace = other.colorSpace;

        buffer = other.buffer;
        other.buffer = nullptr;
        profile = other.profile;
        other.profile = nullptr;
    }

    Bitmap::~Bitmap() {
        delete [] buffer;
        buffer = nullptr;
        cmsCloseProfile(profile);
    }

    Bitmap Bitmap::copy() const {
        size_t size = sizeOfBuffer();
        uint8_t* cb = new uint8_t[size];
        memcpy(cb, buffer, size);
        return Bitmap(width, height, cb, sampleType, colorSpace, cloneProfile(profile));
    }

    Bitmap &Bitmap::operator=(Bitmap &&b) {
        if (this != &b) {
            delete[] buffer;
            cmsCloseProfile(profile);

            width = b.width;
            height = b.height;
            totalSamples = b.totalSamples;
            stride = b.stride;
            samplesPerPixel = b.samplesPerPixel;
            sampleType = b.sampleType;
            colorSpace = b.colorSpace;

            buffer = b.buffer;
            profile = cloneProfile(b.profile);

            b.buffer = nullptr;
            cmsCloseProfile(b.profile);
        }

        return *this;
    }
    Bitmap &Bitmap::operator=(const Bitmap &b) {
        if (this != &b) {
            delete[] buffer;
            cmsCloseProfile(profile);

            width = b.width;
            height = b.height;
            totalSamples = b.totalSamples;
            stride = b.stride;
            samplesPerPixel = b.samplesPerPixel;
            sampleType = b.sampleType;
            colorSpace = b.colorSpace;
            
            size_t size = b.sizeOfBuffer();
            buffer = new uint8_t[size];
            memcpy(buffer, b.buffer, size);
            profile = cloneProfile(b.profile);
        }

        return *this;
    }

    Bitmap Bitmap::convertByLcms2(SampleType newSampleType, ColorSpace newColorSpace) const {
        void* newBuffer = nullptr;
        cmsHPROFILE newProfile = nullptr;

        uint32_t inBytesPerSample  = getBytesPerSample(sampleType);
        uint32_t outBytesPerSample = getBytesPerSample(newSampleType);

        bool inIsFloat  = sampleType == SampleType::F32;
        bool outIsFloat = newSampleType == SampleType::F32;

        auto inType  = colorSpace.buildLcmsType(inBytesPerSample, inIsFloat);
        auto outType = newColorSpace.buildLcmsType(outBytesPerSample, outIsFloat);

        newProfile = colorSpace.createProfile();

        cmsHTRANSFORM t = cmsCreateTransform(
            profile, inType,
            newProfile, outType,
            INTENT_RELATIVE_COLORIMETRIC, 0
        );

        size_t size = width * height;
        const bool haveSameChannelsNumber = colorSpace.isHaveSameChannelsNumber(newColorSpace);
        const bool haveSameDepth = inBytesPerSample == outBytesPerSample;

        if (haveSameChannelsNumber && haveSameDepth) {
            newBuffer = new uint8_t[sizeOfBuffer()]; 
            cmsDoTransform(t, buffer, newBuffer, size);
        } else {
            size_t newBufferSize = size * outBytesPerSample * (size_t)newColorSpace.channels;        
            newBuffer = new uint8_t[newBufferSize];
            cmsDoTransform(t, buffer, newBuffer, size);
        }

        cmsDeleteTransform(t);

        return Bitmap(width, height, newBuffer, newSampleType, newColorSpace, newProfile);
    }

    Bitmap Bitmap::convertTo(const SampleType newSampleType, const ColorSpace newColorSpace, Properties props) const {
        if (sampleType == newSampleType && colorSpace == newColorSpace) return copy();

        if (colorSpace.isRequireProfileReconstruction(newColorSpace)) {
            return convertByLcms2(newSampleType, newColorSpace);
        }

        void* newBuffer = nullptr;

        if (sampleType == SampleType::U8) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<uint8_t, uint8_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<uint8_t, uint16_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<uint8_t, uint32_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<uint8_t, float>(newSampleType, newColorSpace, props);
        }
        if (sampleType == SampleType::U16) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<uint16_t, uint8_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<uint16_t, uint16_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<uint16_t, uint32_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<uint16_t, float>(newSampleType, newColorSpace, props);
        }
        if (sampleType == SampleType::U32) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<uint32_t, uint8_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<uint32_t, uint16_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<uint32_t, uint32_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<uint32_t, float>(newSampleType, newColorSpace, props);
        }
        if (sampleType == SampleType::F32) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<float, uint8_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<float, uint16_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<float, uint32_t>(newSampleType, newColorSpace, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<float, float>(newSampleType, newColorSpace, props);
        }

        return Bitmap(width, height, newBuffer, newSampleType, newColorSpace, cloneProfile(profile));
    }

    size_t Bitmap::getBytesPerSample(const SampleType sampleType) {
        if (sampleType == SampleType::U8) return 1;
        if (sampleType == SampleType::U16) return 2;
        if (sampleType == SampleType::U32) return 4;
        if (sampleType == SampleType::F32) return 4;

        throw std::runtime_error("Unsupported depth");
    }

    std::vector<uint8_t> Bitmap::getICCProfile() const {
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(profile, NULL, &size);

        std::vector<uint8_t> icc(size);
        cmsSaveProfileToMem(profile, icc.data(), &size);
        return icc;
    }
}