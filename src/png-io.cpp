#include "png-io.h"
#include <libpng16/png.h>
#include <lcms2.h>

namespace ImageIO {
    namespace {
        void writeProfileToMem(cmsHPROFILE profile, uint8_t*& icc, uint32_t &iccSize) {
            cmsSaveProfileToMem(profile, NULL, &iccSize);
            icc = new uint8_t[iccSize]();
            cmsSaveProfileToMem(profile, icc, &iccSize);
        }

        void retrieveICCProfile(png_structp png, png_infop info, uint8_t*& icc, uint32_t &iccSize) {
            png_charp    icc_name;
            int          icc_compression;
            png_bytep    icc_data;
            png_uint_32  icc_length;

            if (png_get_iCCP(png, info, &icc_name, &icc_compression, &icc_data, &icc_length) == PNG_INFO_iCCP) {
                iccSize = icc_length;
                icc = new uint8_t[iccSize];
                memcpy(icc, icc_data, icc_length);
                iccSize = icc_length;
                return;
            }
            if (png_get_sRGB(png, info, nullptr) == PNG_INFO_sRGB) {
                auto profile = cmsCreate_sRGBProfile();
                writeProfileToMem(profile, icc, iccSize);
                cmsCloseProfile(profile);
                return;
            }
            
            double wx, wy, rx, ry, gx, gy, bx, by;
            double gamma_value;

            int has_chrm = png_get_cHRM(png, info,
                &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by) & PNG_INFO_cHRM;
            int has_gama = png_get_gAMA(png, info,
                &gamma_value) & PNG_INFO_gAMA;

            if (has_chrm && has_gama) {
                cmsCIExyYTRIPLE primaries = {
                    .Red   = { rx, ry, 1.0 },
                    .Green = { gx, gy, 1.0 },
                    .Blue  = { bx, by, 1.0 }
                };
                cmsCIExyY white_point = { wx, wy, 1.0 };

                cmsToneCurve *curve = cmsBuildGamma(NULL, 1.0 / gamma_value);
                cmsToneCurve *curves[3] = { curve, curve, curve };

                cmsHPROFILE profile = cmsCreateRGBProfile(
                    &white_point, &primaries, curves
                );

                cmsFreeToneCurve(curve);
                writeProfileToMem(profile, icc, iccSize);
                cmsCloseProfile(profile);
            } else if (has_chrm && !has_gama) { // assume sRGB gamma
                cmsCIExyYTRIPLE primaries = {
                    { rx, ry, 1.0 }, { gx, gy, 1.0 }, { bx, by, 1.0 }
                };
                cmsCIExyY white_point = { wx, wy, 1.0 };

                cmsToneCurve *srgb_trc = cmsBuildParametricToneCurve(
                    NULL, 4, (double[]){ 2.4, 1.0/1.055, 0.055/1.055, 1.0/12.92, 0.04045 }
                );
                cmsToneCurve *curves[3] = { srgb_trc, srgb_trc, srgb_trc };

                cmsHPROFILE profile = cmsCreateRGBProfile(
                    &white_point, &primaries, curves
                );
                cmsFreeToneCurve(srgb_trc);
                writeProfileToMem(profile, icc, iccSize);
                cmsCloseProfile(profile);
            } else {
                auto profile = cmsCreate_sRGBProfile();
                writeProfileToMem(profile, icc, iccSize);
                cmsCloseProfile(profile);
            }
        }

        typedef struct {
            uint8_t samplesPerPixel;
            NativeColorSpace colorSpace;
        } PixelFormatInfo;

        PixelFormatInfo resolvePixelFormat(uint8_t colorSpace) {
            switch (colorSpace) {
                case PNG_COLOR_TYPE_GRAY:       return { 1, NativeColorSpace::Grayscale };
                case PNG_COLOR_TYPE_GRAY_ALPHA: return { 2, NativeColorSpace::GrayscaleAlpha };
                case PNG_COLOR_TYPE_RGB:        return { 3, NativeColorSpace::RGB };
                case PNG_COLOR_TYPE_RGB_ALPHA:  return { 4, NativeColorSpace::RGBA };
                default:                        return { 4, NativeColorSpace::RGBA };
            }
        }

        inline int getOutColorType(const Channels channels) {
            switch (channels) {
                case Channels::Grayscale:      return PNG_COLOR_TYPE_GRAY;
                case Channels::GrayscaleAlpha: return PNG_COLOR_TYPE_GA;
                case Channels::RGB:            return PNG_COLOR_TYPE_RGB;
                case Channels::RGBA:           return PNG_COLOR_TYPE_RGBA;
                default:
                    throw std::runtime_error("PNG: Unsupported channels format");
            }
        }
    }

    NativeBitmap loadPNG(const char *filename) {
        FILE *file = fopen(filename, "rb");
        if (file == nullptr) {
            throw std::runtime_error("Failed to open file");
        }

        auto png = png_create_read_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr
        );
        if (!png) {
            fclose(file);
            throw std::runtime_error("Failed to create png struct");
        }
        auto info = png_create_info_struct(png);
        if (!info) {
            png_destroy_read_struct(&png, nullptr, nullptr);
            fclose(file);
            throw std::runtime_error("Failed to create png info struct");
        }

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_read_struct(&png, &info, NULL);
            fclose(file);
            throw std::runtime_error("libpng error: setjmp");
        }

        png_init_io(png, file);
        png_read_info(png, info);
        
        uint8_t bit_depth  = png_get_bit_depth(png, info);
        uint8_t color_type = png_get_color_type(png, info);

        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png);
        }

        png_read_update_info(png, info);

        uint32_t width    = png_get_image_width(png, info);
        uint32_t height   = png_get_image_height(png, info);
        size_t row_stride = png_get_rowbytes(png, info);
        bit_depth         = png_get_bit_depth(png, info);
        color_type        = png_get_color_type(png, info);

        uint8_t* icc = nullptr;
        uint32_t icc_size = 0;;

        retrieveICCProfile(png, info, icc, icc_size);
        
        size_t data_size = row_stride * height;
        uint8_t* data = new uint8_t[data_size];

        uint8_t** rows = new uint8_t*[height];
        for (int y = 0; y < height; y++)
            rows[y] = data + y * row_stride;

        png_read_image(png, rows);
        png_read_end(png, info);

        delete [] rows;
        png_destroy_read_struct(&png, &info, nullptr);
        fclose(file);

        PixelFormatInfo pfi = resolvePixelFormat(color_type);

        return NativeBitmap { 
            width, height, pfi.samplesPerPixel, bit_depth, SampleFormat::UInt, 
            pfi.colorSpace, icc, icc_size, data, data_size
        };
    }

    void savePNG(const char *filename, const Bitmap &bitmap, Properties props) {
        bool supportedDepth = bitmap.sampleType == SampleType::U8 || bitmap.sampleType == SampleType::U16;
        Bitmap src = supportedDepth ? bitmap.copy() : bitmap.convertSampleType(SampleType::U16);

        int colorType = getOutColorType(src.colorSpace.channels);

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) {
            throw std::runtime_error("PNG: png_create_write_struct failed");
        }

        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_write_struct(&png, nullptr);
            throw std::runtime_error("PNG: png_create_info_struct failed");
        }

        FILE* file = fopen(filename, "wb");
        if (!file) {
            png_destroy_write_struct(&png, &info);
            throw std::runtime_error("PNG: cannot open file: " + std::string(filename));
        }

        if (setjmp(png_jmpbuf(png))) {
            fclose(file);
            png_destroy_write_struct(&png, &info);
            throw std::runtime_error("PNG: libpng error during write");
        }

        png_init_io(png, file);

        int bitDepth = src.getBytesPerSample(src.sampleType) * 8;

        png_set_IHDR(
            png, info, src.width, src.height, bitDepth, colorType, 
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, 
            PNG_FILTER_TYPE_DEFAULT
        );

        auto icc = src.getICCProfile();
        png_set_iCCP(
            png, info, "ICC Profile", PNG_COMPRESSION_TYPE_BASE, 
            reinterpret_cast<png_const_bytep>(icc.data()),
            static_cast<png_uint_32>(icc.size())
        );

        png_write_info(png, info);

        if (src.sampleType == SampleType::U16) {
            png_set_swap(png);
        }

        const uint8_t* bufPtr = src.ptr<uint8_t>();
        const size_t rowBytes = static_cast<size_t>(src.stride);

        std::vector<png_bytep> rows(src.height);
        for (uint32_t y = 0; y < src.height; ++y) {
            rows[y] = const_cast<png_bytep>(bufPtr + y * rowBytes);
        }

        png_write_image(png, rows.data());
        png_write_end(png, nullptr);

        fclose(file);
        png_destroy_write_struct(&png, &info);
    }
}
