#pragma once
#include "color-model.h"
#include <vector>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace ImageIO {
    class ColorProfile {
    public:
        ColorProfile() = default;

        static ColorProfile sRGB();
        static ColorProfile AdobeRGB();
        static ColorProfile ProPhotoRGB();
        static ColorProfile CMYK();
        static ColorProfile XYZ();
        static ColorProfile Gray();
        static ColorProfile Default(ColorModel model);

        static ColorProfile FromMemory(const void* icc, size_t size);
        static ColorProfile FromFile(const char* path);

        bool empty() const noexcept { 
            return !impl_;
        }
        explicit operator bool() const noexcept { 
            return impl_ != nullptr;
        }

        std::vector<std::uint8_t> toICC() const;
        void* nativeHandle() const noexcept;
    
    private:
        struct Impl;
        static ColorProfile adopt(void* cmsHandle);
        std::shared_ptr<Impl> impl_;
    };
}