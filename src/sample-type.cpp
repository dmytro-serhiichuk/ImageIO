#include "ImageIO/sample-type.h"
#include <stdexcept>

namespace ImageIO {
    uint8_t getBytesPerSample(SampleType sampleType) {
        switch (sampleType) {
            case SampleType::U8:  
                return 1;
            case SampleType::U16: 
                return 2;
            case SampleType::U32: 
            case SampleType::F32: 
                return 4;
            default:
                throw std::runtime_error("Invalid sample type");
        }
    }
}
