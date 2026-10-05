#include "png-io.h"
#include <libpng16/png.h>
#include "profile-management.h"

namespace ImageIO {
    namespace {
        inline cmsHPROFILE retrieveICCProfile(png_structp png, png_infop info, ColorModel colorModel) {
            png_charp    icc_name;
            int          icc_compression;
            png_bytep    icc_data;
            png_uint_32  icc_length;

            if (png_get_iCCP(png, info, &icc_name, &icc_compression, &icc_data, &icc_length) == PNG_INFO_iCCP) {
                return cmsOpenProfileFromMem(icc_data, icc_length);
            }
            if (png_get_sRGB(png, info, nullptr) == PNG_INFO_sRGB) {
                return cmsCreate_sRGBProfile();
            }
            
            double wx, wy, rx, ry, gx, gy, bx, by;
            double gamma_value;

            int has_chrm = png_get_cHRM(png, info,
                &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by) & PNG_INFO_cHRM;
            int has_gama = png_get_gAMA(png, info,
                &gamma_value) & PNG_INFO_gAMA;

            if (has_chrm && has_gama) {
                cmsCIExyYTRIPLE primaries = {
                    { rx, ry, 1.0 },
                    { gx, gy, 1.0 },
                    { bx, by, 1.0 }
                };
                cmsCIExyY white_point = { wx, wy, 1.0 };

                cmsToneCurve *curve = cmsBuildGamma(NULL, 1.0 / gamma_value);
                cmsToneCurve *curves[3] = { curve, curve, curve };

                return cmsCreateRGBProfile(
                    &white_point, &primaries, curves
                );
            } else if (has_chrm && !has_gama) { // assume sRGB gamma
                cmsCIExyYTRIPLE primaries = {
                    { rx, ry, 1.0 }, { gx, gy, 1.0 }, { bx, by, 1.0 }
                };
                cmsCIExyY white_point = { wx, wy, 1.0 };
                
                double params[5] = { 2.4, 1.0/1.055, 0.055/1.055, 1.0/12.92, 0.04045 };
                cmsToneCurve *srgb_trc = cmsBuildParametricToneCurve(NULL, 4, params);
                cmsToneCurve *curves[3] = { srgb_trc, srgb_trc, srgb_trc };

                return cmsCreateRGBProfile(
                    &white_point, &primaries, curves
                );
            } else {
                return createProfileFromColorModel(colorModel);
            }
        }

        inline ColorModel resolveColorModel(uint8_t colorSpace) {
            switch (colorSpace) {
                case PNG_COLOR_TYPE_GRAY:       return ColorModel::GRAY;
                case PNG_COLOR_TYPE_GRAY_ALPHA: return ColorModel::GRAYA;
                case PNG_COLOR_TYPE_RGB:        return ColorModel::RGB;
                case PNG_COLOR_TYPE_RGB_ALPHA:  return ColorModel::RGBA;
                default:                        return ColorModel::RGBA;
            }
        }

        inline Bitmap convertForSaving(const Bitmap &src) {
            bool supportedDepth = src.sampleType == SampleType::U8 || src.sampleType == SampleType::U16;
            SampleType outSampleType = supportedDepth ? src.sampleType : SampleType::U16;

            switch (src.colorModel) {
                case ColorModel::XYZ: 
                case ColorModel::CMYK:
                case ColorModel::CMYKA:
                    return src.convertTo(outSampleType, ColorModel::RGB);
                default:
                    return src.convertSampleType(outSampleType);
            }
        }

        inline int getOutColorType(const ColorModel colorModel) {
            switch (colorModel) {
                case ColorModel::GRAY:  return PNG_COLOR_TYPE_GRAY;
                case ColorModel::GRAYA: return PNG_COLOR_TYPE_GA;
                case ColorModel::RGB:   return PNG_COLOR_TYPE_RGB;
                case ColorModel::RGBA:  return PNG_COLOR_TYPE_RGBA;
                default:
                    throw std::runtime_error("PNG: Unsupported channels format");
            }
        }
    }

    Bitmap loadPNG(const char *filename) {
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
        if (bit_depth == 16) {
            png_set_swap(png);
        }
        if (bit_depth < 8 && color_type == PNG_COLOR_TYPE_GRAY) {
            png_set_expand_gray_1_2_4_to_8(png);
        }

        png_read_update_info(png, info);

        uint32_t width    = png_get_image_width(png, info);
        uint32_t height   = png_get_image_height(png, info);
        size_t row_stride = png_get_rowbytes(png, info);
        bit_depth         = png_get_bit_depth(png, info);
        color_type        = png_get_color_type(png, info);

        SampleType sampleType = bit_depth == 8 ? SampleType::U8 : SampleType::U16;
        ColorModel colorModel = resolveColorModel(color_type);

        auto profile = retrieveICCProfile(png, info, colorModel);
        
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

        return Bitmap(width, height, data, sampleType, colorModel, profile);
    }

    void savePNG(const char *filename, const Bitmap &bitmap, Properties props) {
        Bitmap src = convertForSaving(bitmap);

        int colorType = getOutColorType(src.colorModel);

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

        int bitDepth = getBytesPerSample(src.sampleType) * 8;

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

        const uint8_t* bufPtr = src.buffer;
        const size_t rowBytes = static_cast<size_t>(src.stride);

        std::vector<png_bytep> rows(src.height);
        for (uint32_t y = 0; y < src.height; ++y) {
            rows[y] = const_cast<png_bytep>(bufPtr + y * rowBytes);
        }

        png_write_image(png, rows.data());
        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);

        fclose(file);
    }
}
