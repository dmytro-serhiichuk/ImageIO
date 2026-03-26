#ifndef IMAGEIO_COLOR_MODEL_H
#define IMAGEIO_COLOR_MODEL_H

#include <cstdint>

namespace ImageIO {
    constexpr uint32_t ALPHA_FLAG = 1;

    enum class ColorModel : uint32_t {
        RGB  = 1 << 1,
        GRAY = 1 << 2,
        CMYK = 1 << 3,
        XYZ  = 1 << 4,

        RGBA  = RGB  | ALPHA_FLAG,
        GRAYA = GRAY | ALPHA_FLAG,
        CMYKA = CMYK | ALPHA_FLAG
    };

    uint8_t getSamplesPerPixel(ColorModel colorModel);

    inline bool hasAlpha(ColorModel colorModel) {
        return ((uint32_t)colorModel & ALPHA_FLAG) != 0;
    }

    bool isColorModelsShareProfiles(const ColorModel a, const ColorModel b);
}

#endif