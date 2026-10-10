#ifndef IMAGEIO_PROPERTIES_H
#define IMAGEIO_PROPERTIES_H

namespace ImageIO {
    struct BackgroundColor {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
    };

    // TODO: blend alpha
    struct Properties {
        bool blendAlpha = false;
        BackgroundColor backgroundColor;
        int jpegQuality = 100;
    };
}

#endif 