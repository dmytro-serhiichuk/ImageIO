#ifndef IMAGEIO_SAMPLE_TYPE_H
#define IMAGEIO_SAMPLE_TYPE_H

#include <cstdint>

namespace ImageIO {
    enum class SampleType {
        U8,
        U16,
        U32,
        F32
    };

    uint8_t getBytesPerSample(SampleType sampleType);
}

#endif