#include "jpeg-io.h"
#include <turbojpeg.h>
#include <type_traits>
#include <stdexcept>

namespace ImageIO {
    namespace
    {
        typedef struct {
            TJPF jpegPixelFormat;
            uint8_t samplesPerPixel;
            NativeColorSpace colorSpace;
        } PixelFormatInfo;

        PixelFormatInfo resolvePixelFormat(TJCS colorSpace) {
            switch (colorSpace) {
                case TJCS_GRAY:  return { TJPF_GRAY, 1, NativeColorSpace::Grayscale };
                case TJCS_RGB:   return { TJPF_RGB,  3, NativeColorSpace::RGB };
                case TJCS_YCbCr: return { TJPF_RGB,  3, NativeColorSpace::RGB };
                case TJCS_CMYK:  return { TJPF_CMYK, 4, NativeColorSpace::CMYK };
                case TJCS_YCCK:  return { TJPF_CMYK, 4, NativeColorSpace::CMYK };
                default:         return { TJPF_RGB,  3, NativeColorSpace::RGB };
            }
        }

        void writeProfileToMem(cmsHPROFILE profile, uint8_t*& icc, uint32_t &iccSize) {
            cmsSaveProfileToMem(profile, NULL, &iccSize);
            icc = new uint8_t[iccSize]();
            cmsSaveProfileToMem(profile, icc, &iccSize);
        }
    }
    
    NativeBitmap loadJPEG(const char* filename) {
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

        size_t iccSize = 0;
        uint8_t* iccBuffer = nullptr;
        if (tj3GetICCProfile(decompressor, &iccBuffer, &iccSize) < 0 && tj3GetErrorCode(decompressor) != 0) {
            tj3Destroy(decompressor);
            throw std::runtime_error("Failed to get icc profile");
        }
        if (iccSize == 0 || iccBuffer == nullptr) {
            uint32_t s = 0;
            auto profile = cmsCreate_sRGBProfile();
            writeProfileToMem(profile, iccBuffer, s);
            cmsCloseProfile(profile);
            iccSize = s;
        }

        size_t bufferSize = width * height * pfi.samplesPerPixel;
        uint8_t* buffer = new uint8_t[bufferSize];
        
        if (tj3Decompress8(decompressor, jpegData.data(), length, buffer, 0, pfi.jpegPixelFormat) < 0) {
            tj3Destroy(decompressor);
            throw std::runtime_error("Failed to decompress jpeg");
        }

        tj3Destroy(decompressor);

        return NativeBitmap { 
            (uint32_t)width, (uint32_t)height, pfi.samplesPerPixel, 
            (uint8_t)precision, SampleFormat::UInt, pfi.colorSpace, 
            iccBuffer, iccSize, buffer, bufferSize
        };
    }

    void saveJPEG(const char* filename, const Bitmap &bitmap, Properties props) {
        Bitmap* convertedBitmap = bitmap.convertDepth(BitmapDepth::U8);

        size_t jpegSize = 0;
        uint8_t* jpegBuf = nullptr;

        tjhandle jpegCompressor = tj3Init(TJINIT_COMPRESS);
        if (!jpegCompressor) {
            throw std::runtime_error("Failed to init jpeg compressor");
        }

        jpegSize = 0;
        jpegBuf = nullptr;

        int pixelFormat = convertedBitmap->colorSpace == BitmapColorSpace::RGB ? TJPF_RGB :
                          convertedBitmap->colorSpace == BitmapColorSpace::RGBA ? TJPF_RGBA :
                          TJPF_GRAY;

        int subsamp = (convertedBitmap->colorSpace == BitmapColorSpace::RGB || 
                      convertedBitmap->colorSpace == BitmapColorSpace::RGBA) 
                      ? TJSAMP_444 
                      : TJSAMP_GRAY;
        uint8_t* buffer = convertedBitmap->ptr<uint8_t>();

        tj3Set(jpegCompressor, TJPARAM_QUALITY, props.jpegQuality);
        tj3Set(jpegCompressor, TJPARAM_SUBSAMP , subsamp);

        auto profile = convertedBitmap->getICCProfile();
        tj3SetICCProfile(jpegCompressor, profile.data(), profile.size());

        if (tj3Compress8(jpegCompressor, buffer, convertedBitmap->width, 0, convertedBitmap->height, pixelFormat, &jpegBuf, &jpegSize) < 0) {
            tj3Destroy(jpegCompressor);
            delete convertedBitmap;
            throw std::runtime_error("Failed to compress jpeg");
        }

        tj3Destroy(jpegCompressor);
  
        FILE* file = fopen(filename, "wb");
        fwrite(jpegBuf, sizeof(uint8_t), jpegSize, file);
        fclose(file);

        tj3Free(jpegBuf);

        delete convertedBitmap;
    }
}
