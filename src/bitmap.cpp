#include "bitmap.h"
#include "profile-management.h"

namespace ImageIO {
    Bitmap::Bitmap(const Bitmap& other) {
        width = other.width;
        height = other.height;
        bufferLength = other.bufferLength;
        stride = other.stride;
        colorSpace = other.colorSpace;
        depth = other.depth;
        colorProfile = other.colorProfile;

        size_t s = other.getBytesPerSample(other.depth);
        buffer = new uint8_t[s];
        memcpy(buffer, other.buffer, s);

        _profile = cloneProfile(other._profile);
    }

    Bitmap::Bitmap(Bitmap&& other) noexcept {
        width = other.width;
        height = other.height;
        bufferLength = other.bufferLength;
        stride = other.stride;
        colorSpace = other.colorSpace;
        depth = other.depth;
        colorProfile = other.colorProfile;

        buffer = other.buffer;
        other.buffer = nullptr;
        _profile = other._profile;
        other._profile = nullptr;
    }

    Bitmap::~Bitmap() {
        delete [] buffer;
        buffer = nullptr;
        cmsCloseProfile(_profile);
    }

    inline Bitmap *Bitmap::copy() const {
        size_t size = bufferLength * Bitmap::getBytesPerSample(depth);
        uint8_t* cb = new uint8_t[size];
        memcpy(cb, buffer, size);
        return new Bitmap(width, height, cb, colorSpace, depth, colorProfile, cloneProfile(_profile));
    }

    Bitmap &Bitmap::operator=(Bitmap &&b) {
        if (this != &b) {
            delete[] buffer;
            cmsCloseProfile(_profile);

            width = b.width;
            height = b.height;
            bufferLength = b.bufferLength;
            stride = b.stride;
            colorSpace = b.colorSpace;
            depth = b.depth;
            colorProfile = b.colorProfile;

            buffer = b.buffer;
            _profile = cloneProfile(b._profile);

            b.buffer = nullptr;
            cmsCloseProfile(b._profile);
        }

        return *this;
    }
    Bitmap &Bitmap::operator=(const Bitmap &b) {
        if (this != &b) {
            delete[] buffer;
            cmsCloseProfile(_profile);

            width = b.width;
            height = b.height;
            bufferLength = b.bufferLength;
            stride = b.stride;
            colorSpace = b.colorSpace;
            depth = b.depth;
            colorProfile = b.colorProfile;
            
            size_t s = b.getBytesPerSample(b.depth);
            buffer = new uint8_t[s];
            memcpy(buffer, b.buffer, s);
            _profile = cloneProfile(b._profile);
        }

        return *this;
    }
    
    Bitmap *Bitmap::convertTo(BitmapDepth newDepth, BitmapColorSpace newColorSpace, Properties props) const {
        if (newDepth == depth && newColorSpace == colorSpace) {
            return copy();
        }

        if (depth == BitmapDepth::U8) {
            if (newDepth == BitmapDepth::U8) return convert<uint8_t, uint8_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U16) return convert<uint8_t, uint16_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U32) return convert<uint8_t, uint32_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::F32) return convert<uint8_t, float>(newDepth, newColorSpace, props);
        }
        if (depth == BitmapDepth::U16) {
            if (newDepth == BitmapDepth::U8) return convert<uint16_t, uint8_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U16) return convert<uint16_t, uint16_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U32) return convert<uint16_t, uint32_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::F32) return convert<uint16_t, float>(newDepth, newColorSpace, props);
        }
        if (depth == BitmapDepth::U32) {
            if (newDepth == BitmapDepth::U8) return convert<uint32_t, uint8_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U16) return convert<uint32_t, uint16_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U32) return convert<uint32_t, uint32_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::F32) return convert<uint32_t, float>(newDepth, newColorSpace, props);
        }
        if (depth == BitmapDepth::F32) {
            if (newDepth == BitmapDepth::U8) return convert<float, uint8_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U16) return convert<float, uint16_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::U32) return convert<float, uint32_t>(newDepth, newColorSpace, props);
            if (newDepth == BitmapDepth::F32) return convert<float, float>(newDepth, newColorSpace, props);
        }

        throw std::runtime_error("Unsupported type");
    }

    size_t Bitmap::getBytesPerSample(BitmapDepth depth) {
        if (depth == BitmapDepth::U8) return 1;
        if (depth == BitmapDepth::U16) return 2;
        if (depth == BitmapDepth::U32) return 4;
        if (depth == BitmapDepth::F32) return 4;

        throw std::runtime_error("Unsupported depth");
    }

    std::vector<uint8_t> Bitmap::getICCProfile() {
        cmsUInt32Number size = 0;
        cmsSaveProfileToMem(_profile, NULL, &size);

        std::vector<uint8_t> icc(size);
        cmsSaveProfileToMem(_profile, icc.data(), &size);
        return icc;
    }

    Bitmap *Bitmap::convertProfile(BitmapColorProfile newProfile) {
        if (newProfile == colorProfile) return copy();

        size_t bufferSize = sizeOfBuffer();
        auto newBuffer = new uint8_t[bufferSize];
        
        auto format = getProfileFormat();
        auto dstProfile = getCmsHPROFILE(newProfile);
        auto t = cmsCreateTransform(_profile, format, dstProfile, format, INTENT_RELATIVE_COLORIMETRIC, 0);
        cmsDoTransform(t, buffer, newBuffer, width * height);

        cmsCloseProfile(_profile);
        cmsDeleteTransform(t);
        return new Bitmap(width, height, newBuffer, colorSpace, depth, newProfile, dstProfile);
    }

    cmsUInt32Number Bitmap::getProfileFormat() {
        if (colorSpace == BitmapColorSpace::RGB) {
            switch (depth) {
                case BitmapDepth::U8:  return TYPE_RGB_8;
                case BitmapDepth::U16: return TYPE_RGB_16;
                case BitmapDepth::F32: return TYPE_RGB_FLT;
            }
        }
        if (colorSpace == BitmapColorSpace::Grayscale) {
            switch (depth) {
                case BitmapDepth::U8:  return TYPE_GRAY_8;
                case BitmapDepth::U16: return TYPE_GRAY_16;
                case BitmapDepth::F32: return TYPE_GRAY_FLT;
            }
        }
        if (colorSpace == BitmapColorSpace::RGBA) {
            switch (depth) {
                case BitmapDepth::U8:  return TYPE_RGBA_8;
                case BitmapDepth::U16: return TYPE_RGBA_16;
                case BitmapDepth::F32: return TYPE_RGBA_FLT;
            }
        }
        throw std::runtime_error("No LCMS2 type for this combination");
    }
}