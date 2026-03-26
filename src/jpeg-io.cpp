#include "jpeg-io.h"
#include <turbojpeg.h>
#include <type_traits>
#include <stdexcept>
#include "profile-management.h"

namespace ImageIO {
    namespace {
        typedef struct {
            TJPF jpegPixelFormat;
            ColorModel colorModel;
        } PixelFormatInfo;

        PixelFormatInfo resolvePixelFormat(TJCS colorSpace) {
            switch (colorSpace) {
                case TJCS_GRAY:  return { TJPF_GRAY, ColorModel::GRAY };
                case TJCS_RGB:   return { TJPF_RGB,  ColorModel::RGB };
                case TJCS_YCbCr: return { TJPF_RGB,  ColorModel::RGB };
                case TJCS_CMYK:  return { TJPF_CMYK, ColorModel::CMYK };
                case TJCS_YCCK:  return { TJPF_CMYK, ColorModel::CMYK };
                default:         return { TJPF_RGB,  ColorModel::RGB };
            }
        }

        cmsHPROFILE retrieveICCProfile(tjhandle decompressor, ColorModel colorModel) {
            size_t iccSize = 0;
            uint8_t* iccBuffer = nullptr;
            
            struct Guard { uint8_t* b; ~Guard() { delete [] b; } } guard{iccBuffer}; 

            if (tj3GetICCProfile(decompressor, &iccBuffer, &iccSize) < 0 && tj3GetErrorCode(decompressor) != 0) {
                tj3Destroy(decompressor);
                throw std::runtime_error("Failed to get icc profile");
            }


            if (iccBuffer != nullptr && iccSize != 0) {
                return cmsOpenProfileFromMem(iccBuffer, iccSize);
            } else {
                if (colorModel == ColorModel::RGB) {
                    return cmsCreate_sRGBProfile();
                } else if (colorModel == ColorModel::GRAY) {
                    return createDefaultGrayProfile();
                } else {
                    return createCMYKProfile();
                }
            }
        }

        Bitmap convertForSaving(const Bitmap &src) {
            if (src.colorModel == ColorModel::GRAYA) {
                return src.convertTo(SampleType::U8, ColorModel::GRAY);
            } else if (src.colorModel == ColorModel::XYZ) {
                return src.convertTo(SampleType::U8, ColorModel::RGB);
            } else if (src.colorModel == ColorModel::CMYKA) {
                return src.convertTo(SampleType::U8, ColorModel::CMYK);
            } else {
                return src.convertSampleType(SampleType::U8);
            }
        }

        int getColorSpace(const ColorModel colorModel) {
            switch (colorModel) {
                case ColorModel::RGB:
                case ColorModel::RGBA:
                    return TJCS_RGB;
                case ColorModel::GRAY:
                case ColorModel::GRAYA:
                    return TJCS_GRAY;
                case ColorModel::CMYK:
                case ColorModel::CMYKA:
                    return TJCS_YCCK;
                default:
                    throw std::runtime_error("JPG: Unsupported color model");
            } 
        }

        int getPixelFormat(const ColorModel colorModel) {
            switch (colorModel) {
                case ColorModel::RGB:
                    return TJPF_RGB;
                case ColorModel::RGBA:
                    return TJPF_RGBA;
                case ColorModel::GRAY:
                    return TJPF_GRAY;
                case ColorModel::CMYK:
                    return TJPF_CMYK;
                default:
                    throw std::runtime_error("JPG: Unsupported color model");
            }
        }

        int getSubSamp(const ColorModel colorModel) {
            switch (colorModel) {
                case ColorModel::RGB:
                case ColorModel::RGBA:
                case ColorModel::CMYK:
                case ColorModel::CMYKA:
                    return TJSAMP_444;
                case ColorModel::GRAY:
                case ColorModel::GRAYA:
                    return TJSAMP_GRAY;
                default:
                    throw std::runtime_error("JPG: Invalid channels configuration");
            }
        }
    }
    
    // TODO: fix cmyk colors
    Bitmap loadJPEG(const char* filename) {
        FILE *file = fopen(filename, "rb");
        if (file == nullptr) {
            throw std::runtime_error("Failed to open file");
        }

        fseek(file, 0, SEEK_END);
        size_t length = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::vector<uint8_t> jpegData(length);

        if (fread(jpegData.data(), 1, length, file) != length) {
            fclose(file);
            throw std::runtime_error("Failed to read file");
        }
        fclose(file);

        tjhandle decompressor = tj3Init(TJINIT_DECOMPRESS); 
        if (decompressor == nullptr) {
            throw std::runtime_error("Failed to init jpeg decompressor");
        }
        tj3Set(decompressor, TJPARAM_SAVEMARKERS, 2);

        if (tj3DecompressHeader(decompressor, jpegData.data(), length) < 0) {
            tj3Destroy(decompressor);
            throw std::runtime_error("Failed to decompress jpeg header");
        }

        int32_t width     = tj3Get(decompressor, TJPARAM_JPEGWIDTH);
        int32_t height    = tj3Get(decompressor, TJPARAM_JPEGHEIGHT);
        int32_t precision = tj3Get(decompressor, TJPARAM_PRECISION);
        TJCS colorSpace   = (TJCS)tj3Get(decompressor, TJPARAM_COLORSPACE);

        PixelFormatInfo pfi = resolvePixelFormat(colorSpace);

        auto samplesPerPixel = getSamplesPerPixel(pfi.colorModel);
        size_t bufferSize = width * height * samplesPerPixel;
        uint8_t* buffer = new uint8_t[bufferSize];
        
        if (tj3Decompress8(decompressor, jpegData.data(), length, buffer, 0, pfi.jpegPixelFormat) < 0) {
            tj3Destroy(decompressor);
            throw std::runtime_error("Failed to decompress jpeg");
        }

        cmsHPROFILE profile = retrieveICCProfile(decompressor, pfi.colorModel);

        tj3Destroy(decompressor);

        return Bitmap(
            (uint32_t)width, (uint32_t)height, 
            buffer, SampleType::U8, pfi.colorModel, profile
        );
    }

    void saveJPEG(const char* filename, const Bitmap &bitmap, Properties props) {
        Bitmap convertedBitmap = convertForSaving(bitmap);

        size_t jpegSize = 0;
        uint8_t* jpegBuf = nullptr;

        tjhandle jpegCompressor = tj3Init(TJINIT_COMPRESS);
        if (!jpegCompressor) {
            throw std::runtime_error("Failed to init jpeg compressor");
        }

        jpegSize = 0;
        jpegBuf = nullptr;

        int pixelFormat = getPixelFormat(convertedBitmap.colorModel);
        int subsamp = getSubSamp(convertedBitmap.colorModel);
        int colorSpace = getColorSpace(convertedBitmap.colorModel);

        tj3Set(jpegCompressor, TJPARAM_QUALITY, props.jpegQuality);
        tj3Set(jpegCompressor, TJPARAM_SUBSAMP , subsamp);
        tj3Set(jpegCompressor, TJPARAM_COLORSPACE, colorSpace);

        auto profile = convertedBitmap.getICCProfile();
        tj3SetICCProfile(jpegCompressor, profile.data(), profile.size());

        if (tj3Compress8(jpegCompressor, convertedBitmap.buffer, convertedBitmap.width, 0, convertedBitmap.height, pixelFormat, &jpegBuf, &jpegSize) < 0) {
            tj3Destroy(jpegCompressor);
            throw std::runtime_error("Failed to compress jpeg");
        }

        tj3Destroy(jpegCompressor);
  
        FILE* file = fopen(filename, "wb");
        fwrite(jpegBuf, sizeof(uint8_t), jpegSize, file);
        fclose(file);

        tj3Free(jpegBuf);
    }
}
