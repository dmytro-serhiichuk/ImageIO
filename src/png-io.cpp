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
        const Bitmap* bmpPtr = &bitmap;
        if (bitmap.depth != BitmapDepth::U8) {
            bmpPtr = bitmap.convertDepth(BitmapDepth::U16);
        }

        png_image image;
        memset(&image, 0, sizeof(image));
        image.version = PNG_IMAGE_VERSION;

        image.width = bitmap.width;
        image.height = bitmap.height;

        image.format = 0;
        if (bmpPtr->colorSpace == BitmapColorSpace::RGB || bmpPtr->colorSpace == BitmapColorSpace::RGBA) {
            image.format |= PNG_FORMAT_FLAG_COLOR;
        }
        if (bmpPtr->colorSpace == BitmapColorSpace::RGBA) {
            image.format |= PNG_FORMAT_FLAG_ALPHA;
        }
        if (bmpPtr->depth != BitmapDepth::U8) {
            image.format |= PNG_FORMAT_FLAG_LINEAR;

            // double gamma = 2.2;
            // uint16_t* buffer16 = bmpPtr->ptr<uint16_t>();
            // for (size_t i = 0; i < bmpPtr->bufferLength; i++) {
            //     buffer16[i] = pow(buffer16[i] / 65535.0, gamma) * 65535.0;
            // }
        }

        if (!png_image_write_to_file(&image, filename, 0, bmpPtr->ptr<uint8_t>(), 0, nullptr)) {
            png_image_free(&image);
            if (bmpPtr != &bitmap) delete bmpPtr;
            throw std::runtime_error("Failed to write to file");
        }

        png_image_free(&image);

        if (bmpPtr != &bitmap) delete bmpPtr;
    }
}
