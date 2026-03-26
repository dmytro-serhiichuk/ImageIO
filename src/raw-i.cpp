#include "raw-i.h"
#include <libraw/libraw.h>
#include <lcms2.h>

namespace ImageIO {
    namespace {
        cmsHPROFILE retrieveICCProfile(const LibRaw &processor) {
            cmsHPROFILE hProfile = cmsCreateProfilePlaceholder(nullptr);
            cmsSetDeviceClass(hProfile, cmsSigColorSpaceClass);
            cmsSetColorSpace(hProfile, cmsSigXYZData);
            cmsSetPCS(hProfile, cmsSigXYZData);

            cmsCIExyY wpxyY;
            cmsWhitePointFromTemp(&wpxyY, 6504.0);
            cmsCIEXYZ wpXYZ;
            cmsxyY2XYZ(&wpXYZ, &wpxyY);
            cmsWriteTag(hProfile, cmsSigMediaWhitePointTag, &wpXYZ);

            {
                const cmsFloat64Number kBradford_D65_to_D50[9] = {
                    1.0478112,  0.0228866, -0.0501270,
                    0.0295424,  0.9904844, -0.0170491,
                    -0.0092345,  0.0150436,  0.7521316
                };

                cmsPipeline* lut = cmsPipelineAlloc(nullptr, 3, 3);
                cmsStage* mat = cmsStageAllocMatrix(nullptr, 3, 3, kBradford_D65_to_D50, nullptr);
                cmsPipelineInsertStage(lut, cmsAT_END, mat);
                cmsWriteTag(hProfile, cmsSigAToB0Tag, lut);
                cmsPipelineFree(lut);
            }

            {
                const cmsFloat64Number kBradford_D50_to_D65[9] = {
                    0.9554734, -0.0230531,  0.0631633,
                    -0.0282525,  1.0099416,  0.0210369,
                    0.0123043, -0.0205345,  1.3303259
                };

                cmsPipeline* lut = cmsPipelineAlloc(nullptr, 3, 3);
                cmsStage* mat = cmsStageAllocMatrix(nullptr, 3, 3, kBradford_D50_to_D65, nullptr);
                cmsPipelineInsertStage(lut, cmsAT_END, mat);
                cmsWriteTag(hProfile, cmsSigBToA0Tag, lut);
                cmsPipelineFree(lut);
            }
            
            return hProfile;
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

    Bitmap loadRAW(const char *file) {
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

        auto profile = retrieveICCProfile(processor);

        uint32_t bigWidth  = processed_image->width;
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

        if (cropWidth != 0 && cropHeight != 0 && !(cropWidth == bigWidth && cropHeight == bigHeight)) {
            const size_t channels = 3;
            auto src = reinterpret_cast<uint16_t *>(processed_image->data);
            buffer = new uint16_t[cropWidth * cropHeight * channels];

            const size_t srcRow = bigWidth * channels;
            const size_t dstRow = cropWidth * channels;

            size_t srcOffset = topOffset * srcRow;

            const uint32_t leftSrcOffset = leftOffset * channels;

            for (size_t i = 0; i < cropHeight; i++) {
                memcpy(buffer + dstRow * i, src + srcOffset + leftSrcOffset, dstRow * sizeof(uint16_t));
                srcOffset += srcRow;
            }

            outputWidth = cropWidth;
            outputHeight = cropHeight;
        }
        else {
            buffer = new uint16_t[processed_image->data_size];
            std::memcpy(buffer, processed_image->data, processed_image->data_size * sizeof(uint8_t));

            outputWidth = bigWidth;
            outputHeight = bigHeight;
        }

        processor.dcraw_clear_mem(processed_image);
        processor.recycle();

        return Bitmap(
            outputWidth, outputHeight, buffer, 
            SampleType::U16, ColorModel::XYZ, profile
        );
    }
}
