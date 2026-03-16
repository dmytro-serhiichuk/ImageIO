#ifndef IMAGEIO_IMAGEIO_H
#define IMAGEIO_IMAGEIO_H

#include "native-bitmap.h"

namespace ImageIO {
    enum class Format {
        JPG,
        PNG,
        RAW,
        TIFF,
        UNDEFINED
    };
    
    /// @brief Extract file format from the file
    /// @param filename Path to the file 
    /// @return Image format or Format::UNDEFINED if the file is not an image or
    /// not a supported image type
    Format getFormat(const char *filename);

    /// @brief Opens image from file and keeps its data as it is in the file
    /// @param filename Path to the file
    /// @return New instance of NativeBitmap
    NativeBitmap openNative(const char* filename);

    /// @brief Opens image from file and recognizes the image type 
    /// @param colorSpace Color space of output bitmap
    /// @param depth Color depth of output bitmap 
    /// @return New Bitmap or nullptr if input image type is not supported    
    Bitmap* open(const char* filename, 
        BitmapColorSpace colorSpace = BitmapColorSpace::RGB, 
        BitmapDepth depth = BitmapDepth::U8, 
        BitmapColorProfile colorProfile = BitmapColorProfile::sRGB_2_2, 
        Properties props = {}
    );

    /// @brief Saves image to the file 
    /// @note automatically defines the output type by extension 
    void save(const char* filename, Bitmap &bitmap, Properties props = {});
}

#endif //IMAGEIO_IMAGEIO_H