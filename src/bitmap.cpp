#include "bitmap.h"
#include "profile-management.h"

namespace ImageIO {
    Bitmap::Bitmap() {
        width = 0;
        height = 0;
        bufferSize = 0;
        totalSamples = 0;
        stride = 0;
        sampleType = SampleType::U8;
        colorModel = ColorModel::RGB;

        buffer = nullptr;
        profile = nullptr;
    }

    Bitmap::Bitmap(const Bitmap &other) {
        width = other.width;
        height = other.height;
        bufferSize = other.bufferSize;
        totalSamples = other.totalSamples;
        stride = other.stride;
        sampleType = other.sampleType;
        colorModel = other.colorModel;

        buffer = new uint8_t[bufferSize];
        memcpy(buffer, other.buffer, bufferSize);
        profile = cloneProfile(other.profile);
    }
    Bitmap::Bitmap(Bitmap &&other) noexcept {
        width = other.width;
        height = other.height;
        bufferSize = other.bufferSize;
        totalSamples = other.totalSamples;
        stride = other.stride;
        sampleType = other.sampleType;
        colorModel = other.colorModel;

        buffer = other.buffer;
        other.buffer = nullptr;
        profile = other.profile;
        other.profile = nullptr;
    }
    Bitmap::~Bitmap() {
        delete [] buffer;
        buffer = nullptr;
        cmsCloseProfile(profile);
        profile = nullptr;
    }

    Bitmap Bitmap::copy() const {
        uint8_t* cb = new uint8_t[bufferSize];
        memcpy(cb, buffer, bufferSize);
        return Bitmap(width, height, cb, sampleType, colorModel, cloneProfile(profile));
    }
  
    Bitmap &Bitmap::operator=(const Bitmap &other) {
        if (this != &other) {
            delete [] buffer;
            cmsCloseProfile(profile);

            width = other.width;
            height = other.height;
            bufferSize = other.bufferSize;
            totalSamples = other.totalSamples;
            stride = other.stride;
            sampleType = other.sampleType;
            colorModel = other.colorModel;

            buffer = new uint8_t[bufferSize];
            memcpy(buffer, other.buffer, bufferSize);
            profile = cloneProfile(other.profile);
        }
        return *this;
    }
    Bitmap &Bitmap::operator=(Bitmap &&other) {
        if (this != &other) {
            delete [] buffer;
            cmsCloseProfile(profile);

            width = other.width;
            height = other.height;
            bufferSize = other.bufferSize;
            totalSamples = other.totalSamples;
            stride = other.stride;
            sampleType = other.sampleType;
            colorModel = other.colorModel;

            buffer = other.buffer;
            other.buffer = nullptr;
            profile = other.profile;
            other.profile = nullptr;
        }
        return *this;
    }

    Bitmap Bitmap::convertByLcms2(SampleType newSampleType, ColorModel newColorModel, const cmsHPROFILE newProfile) const {
        uint8_t* newBuffer = nullptr;
        cmsHPROFILE outProfile = nullptr;

        uint32_t inBytesPerSample  = getBytesPerSample(sampleType);
        uint32_t outBytesPerSample = getBytesPerSample(newSampleType);

        auto inType  = buildLcmsType(colorModel, sampleType);
        auto outType = buildLcmsType(newColorModel, newSampleType);
        
        outProfile = newProfile ? newProfile : createProfileFromColorModel(newColorModel);

        if (!outProfile) {
            throw std::runtime_error("Bitmap: Failed to create output profile");
        }

        cmsHTRANSFORM t = cmsCreateTransform(
            profile, inType,
            outProfile, outType,
            INTENT_RELATIVE_COLORIMETRIC, 0
        );

        if (t == nullptr) {
            cmsCloseProfile(outProfile);
            cmsDeleteTransform(t);
            throw std::runtime_error("Bitmap: Invalid output icc profile");
        }

        size_t size = width * height;
        const uint8_t inSpp  = getSamplesPerPixel(colorModel);
        const uint8_t outSpp = getSamplesPerPixel(newColorModel);
        const bool haveSameChannelsNumber = inSpp == outSpp;
        const bool haveSameDepth = inBytesPerSample == outBytesPerSample;

        if (haveSameChannelsNumber && haveSameDepth) {
            newBuffer = new uint8_t[bufferSize]; 
            cmsDoTransform(t, buffer, newBuffer, size);
        } else {
            size_t newBufferSize = size * outBytesPerSample * outSpp;        
            newBuffer = new uint8_t[newBufferSize];
            if (hasAlpha(newColorModel)) std::fill(newBuffer, newBuffer + newBufferSize, 255);
            cmsDoTransform(t, buffer, newBuffer, size);
        }

        cmsDeleteTransform(t);

        return Bitmap(width, height, newBuffer, newSampleType, newColorModel, outProfile);
    }

    Bitmap Bitmap::convertTo(const SampleType newSampleType, const ColorModel newColorModel, const cmsHPROFILE newProfile, Properties props) const {
        if (sampleType == newSampleType && colorModel == newColorModel) return copy();

        if (!isColorModelsShareProfiles(colorModel, newColorModel) || newProfile != nullptr) {
            return convertByLcms2(newSampleType, newColorModel, newProfile);
        }

        void* newBuffer = nullptr;

        if (sampleType == SampleType::U8) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<uint8_t, uint8_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<uint8_t, uint16_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<uint8_t, uint32_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<uint8_t, float>(newSampleType, newColorModel, props);
        }
        if (sampleType == SampleType::U16) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<uint16_t, uint8_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<uint16_t, uint16_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<uint16_t, uint32_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<uint16_t, float>(newSampleType, newColorModel, props);
        }
        if (sampleType == SampleType::U32) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<uint32_t, uint8_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<uint32_t, uint16_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<uint32_t, uint32_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<uint32_t, float>(newSampleType, newColorModel, props);
        }
        if (sampleType == SampleType::F32) {
            if (newSampleType == SampleType::U8)  newBuffer = _convertTo<float, uint8_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U16) newBuffer = _convertTo<float, uint16_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::U32) newBuffer = _convertTo<float, uint32_t>(newSampleType, newColorModel, props);
            if (newSampleType == SampleType::F32) newBuffer = _convertTo<float, float>(newSampleType, newColorModel, props);
        }

        return Bitmap(width, height, newBuffer, newSampleType, newColorModel, cloneProfile(profile));
    }

    std::vector<uint8_t> Bitmap::getICCProfile() const {
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(profile, NULL, &size);

        std::vector<uint8_t> icc(size);
        cmsSaveProfileToMem(profile, icc.data(), &size);
        return icc;
    }
}