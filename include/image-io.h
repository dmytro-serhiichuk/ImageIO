#ifndef IMAGEIO_IMAGEIO_H
#define IMAGEIO_IMAGEIO_H

#include "bitmap.h"

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

    /// @brief Opens image from file and recognizes the image type 
    /// @param sampleType Sample type and depth of output bitmap 
    /// @param colorModel Channels format and color profile of output bitmap
    /// @param iccProfile ICC Profile of the image
    /// @return New instance of Bitmap
    /// @note The resulted instance takes the ownership of the iccProfile
    /// @note If iccProfile is nullptr the default profile is applied
    Bitmap open(const char* filename, 
        SampleType sampleType = SampleType::U8, 
        ColorModel colorModel = ColorModel::RGB,
        cmsHPROFILE iccProfile = nullptr,
        Properties props = {}
    );

    /// @brief Saves image to the file 
    /// @note automatically defines the output type by extension 
    void save(const char* filename, Bitmap &bitmap, Properties props = {});
}

#endif //IMAGEIO_IMAGEIO_H