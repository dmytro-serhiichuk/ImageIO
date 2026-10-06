#include "ImageIO/bitmap.h"
#include "lcms-utils.h"

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
        profile = other.profile;
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
        profile = std::move(other.profile);
    }
    Bitmap::~Bitmap() {
        delete [] buffer;
        buffer = nullptr;
    }

    Bitmap Bitmap::copy() const {
        return Bitmap(*this);
    }
  
    Bitmap &Bitmap::operator=(const Bitmap &other) {
        if (this != &other) {
            
            width = other.width;
            height = other.height;
            bufferSize = other.bufferSize;
            totalSamples = other.totalSamples;
            stride = other.stride;
            sampleType = other.sampleType;
            colorModel = other.colorModel;
            
            auto newBuffer = new uint8_t[bufferSize];
            memcpy(newBuffer, other.buffer, bufferSize);
            delete [] buffer;
            buffer = newBuffer;

            profile = other.profile;
        }
        return *this;
    }
    Bitmap &Bitmap::operator=(Bitmap &&other) noexcept {
        if (this != &other) {
            delete [] buffer;

            width = other.width;
            height = other.height;
            bufferSize = other.bufferSize;
            totalSamples = other.totalSamples;
            stride = other.stride;
            sampleType = other.sampleType;
            colorModel = other.colorModel;

            buffer = other.buffer;
            other.buffer = nullptr;
            profile = std::move(other.profile);
        }
        return *this;
    }

    Bitmap Bitmap::convertByLcms2(SampleType newSampleType, ColorModel newColorModel, const ColorProfile& newProfile) const {
        if (profile.empty()) {
            throw std::runtime_error("Bitmap: source bitmap has no ICC profile");
        }

        const ColorProfile outProfile = newProfile.empty() ? ColorProfile::Default(newColorModel) : newProfile;
        
        uint8_t* newBuffer = nullptr;
        uint32_t inBytesPerSample  = getBytesPerSample(sampleType);
        uint32_t outBytesPerSample = getBytesPerSample(newSampleType);

        auto inType  = buildLcmsType(colorModel, sampleType);
        auto outType = buildLcmsType(newColorModel, newSampleType);

        cmsHTRANSFORM t = cmsCreateTransform(
            getNativeLcmsProfile(profile), inType,
            getNativeLcmsProfile(outProfile), outType,
            INTENT_RELATIVE_COLORIMETRIC, 0
        );

        if (!t) {
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

    Bitmap Bitmap::convertTo(const SampleType newSampleType, const ColorModel newColorModel, const ColorProfile& newProfile, Properties props) const {
        if (sampleType == newSampleType && colorModel == newColorModel && newProfile.empty()) return copy();
        
        const ColorProfile target = newProfile.empty() ? ColorProfile::Default(newColorModel) : newProfile;

        if (!isColorModelsShareProfiles(colorModel, newColorModel) || 
            !profileEqual(getNativeLcmsProfile(target), getNativeLcmsProfile(profile))) {
            return convertByLcms2(newSampleType, newColorModel, target);
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

        return Bitmap(width, height, newBuffer, newSampleType, newColorModel, profile);
    }
}