#include "ImageIO/color-model.h"
#include <stdexcept>

namespace ImageIO {
    uint8_t getSamplesPerPixel(ColorModel colorModel) {
        switch (colorModel) {
            case ColorModel::GRAY:
                return 1;
            case ColorModel::GRAYA:
                return 2;
            case ColorModel::RGB:
            case ColorModel::XYZ:
                return 3;
            case ColorModel::RGBA:
            case ColorModel::CMYK:
                return 4;
            case ColorModel::CMYKA:
                return 5;
            default:
                throw std::runtime_error("Invalid color model");
        }
    }
    bool isColorModelsShareProfiles(const ColorModel a, const ColorModel b) {
        ColorModel baseA = (ColorModel)((uint32_t)a & ~ALPHA_FLAG);
        ColorModel baseB = (ColorModel)((uint32_t)b & ~ALPHA_FLAG);

        return baseA == baseB;
    }
}
