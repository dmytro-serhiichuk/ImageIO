#include "raw-i.h"
#include <libraw/libraw.h>
#include <lcms2.h>

namespace ImageIO {
    namespace {
        void writeProfileToMem(cmsHPROFILE profile, uint8_t*& icc, uint32_t &iccSize) {
            cmsSaveProfileToMem(profile, NULL, &iccSize);
            icc = new uint8_t[iccSize]();
            cmsSaveProfileToMem(profile, icc, &iccSize);
        }

        void retrieveICCProfile(const LibRaw &processor, uint8_t*& icc, uint32_t &iccSize) {
            const cmsCIExyYTRIPLE colorants = {
                {1.0, 0.0, 0.0},
                {0.0, 1.0, 0.0},
                {0.0, 0.0, 1.0}
            };
            
            cmsToneCurve* lin = cmsBuildGamma(NULL, 1.0);
            cmsToneCurve* curves[3] = {lin, lin, lin};
            
            cmsCIExyY d65;
            cmsWhitePointFromTemp(&d65, 6504.0);
            
            cmsHPROFILE h = cmsCreateRGBProfile(&d65, &colorants, curves);
            cmsFreeToneCurve(lin);
            writeProfileToMem(h, icc, iccSize);
            cmsCloseProfile(h);
        }
    }

    bool isRAW(const char *filename)
    {
        LibRaw processor;
        if (processor.open_file(filename) == LIBRAW_SUCCESS) {
            processor.recycle();
            return true;
        }
        processor.recycle();
        return false;
    }

    NativeBitmap loadRAW(const char *file)
    {
        LibRaw processor;
        if (processor.open_file(file) != LIBRAW_SUCCESS) {
            processor.recycle();
            throw std::runtime_error("RAW: Cannot open input file");
        }
        
        processor.imgdata.params.output_bps = 16;
        processor.imgdata.params.use_camera_wb = 1;
        processor.imgdata.params.use_camera_matrix = 1;
        processor.imgdata.params.no_auto_bright = 1;

        processor.imgdata.params.gamm[0] = 1.0;
        processor.imgdata.params.gamm[1] = 1.0;
        processor.imgdata.params.output_color = LIBRAW_COLORSPACE_ICC;

        if (processor.unpack() != LIBRAW_SUCCESS) {
            throw std::runtime_error("RAW: Cannot unpack input image");            
        }
        if (processor.dcraw_process() != LIBRAW_SUCCESS) {
            throw std::runtime_error("RAW: dcraw_process failed");
        }

        libraw_processed_image_t* processed_image = processor.dcraw_make_mem_image();
        if (!processed_image) {
            throw std::runtime_error("RAW: Getting processed image failed");
        }

        uint8_t* icc = nullptr;
        uint32_t iccSize = 0;
        retrieveICCProfile(processor, icc, iccSize);

        uint32_t bigWidth = processed_image->width;
        uint32_t bigHeight = processed_image->height;

        uint32_t leftOffset   = processor.imgdata.sizes.raw_inset_crops[0].cleft - processor.imgdata.sizes.left_margin;
        uint32_t topOffset    = processor.imgdata.sizes.raw_inset_crops[0].ctop - processor.imgdata.sizes.top_margin;

        uint32_t cropWidth    = processor.imgdata.sizes.raw_inset_crops[0].cwidth;
        uint32_t cropHeight   = processor.imgdata.sizes.raw_inset_crops[0].cheight;

        uint32_t rightOffset  = bigWidth - cropWidth - leftOffset;
        uint32_t bottomOffset = bigHeight - cropHeight - topOffset;

        auto orientation = processor.imgdata.sizes.flip;
        if (orientation == 3) { // 180 deg
            leftOffset = rightOffset;
            topOffset = bottomOffset;
        } else if (orientation == 5) { // 90 deg counter-clockwise
            leftOffset = topOffset;
            topOffset = rightOffset;
            uint32_t cw = cropWidth;
            cropWidth = cropHeight;
            cropHeight = cw;
        } else if (orientation == 6) { // 90 deg clockwise
            topOffset = leftOffset;
            leftOffset = bottomOffset;
            uint32_t cw = cropWidth;
            cropWidth = cropHeight;
            cropHeight = cw;
        }

        uint32_t outputWidth, outputHeight;
        uint16_t* buffer = nullptr;

        if (cropWidth != 0 && cropHeight != 0) {
            uint16_t *src = reinterpret_cast<uint16_t *>(processed_image->data);
            buffer = new uint16_t[cropWidth * cropHeight * 3];
            size_t offset = topOffset * bigWidth * 3;

            uint32_t startOffset = leftOffset * 3;
            uint32_t row = cropWidth * 3;
            uint32_t backOffset = (bigWidth - cropWidth - leftOffset) * 3;

            for (size_t i = 0; i < cropHeight; i++) {
                offset += startOffset;
                memcpy(buffer + row * i, src + offset + row * i, row * sizeof(uint16_t));
                offset += backOffset;
            }
        }
        else {
            buffer = new uint16_t[processed_image->data_size];
            std::memcpy(buffer, processed_image->data, processed_image->data_size * sizeof(uint8_t));

            outputWidth = bigWidth;
            outputHeight = bigHeight;
        }

        processor.dcraw_clear_mem(processed_image);
        processor.recycle();

        return NativeBitmap { 
            outputWidth, outputHeight, 3, 16, SampleFormat::UInt, 
            NativeColorSpace::RGB, icc, iccSize, (uint8_t*)buffer, 
            outputWidth * outputHeight * 3
        };
    }
}
