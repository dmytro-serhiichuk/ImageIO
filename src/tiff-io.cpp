#include "tiff-io.h"
#include <tiffio.h>
#include <lcms2.h>
#include "ImageIO/color-profile.h"

namespace ImageIO {
    namespace {
        enum class SampleFormat {
            UInt, Int, Float
        };

        enum class ColorSpace {
            Unknown, RGB, RGBA, Gray, GrayA, CMYK, CMYKA
        };

        struct TIFFData {
            uint32_t width = 0;
            uint32_t height = 0;
            uint8_t samplesPerPixel = 0;
            uint8_t bitsPerSample = 0;
            SampleFormat sampleFormat;
            ColorSpace colorSpace;

            uint8_t* data = nullptr;
            size_t dataSize = 0;

            ~TIFFData() {
                delete [] data;
            }
        };

        inline SampleFormat getSampleFormat(TIFF *tiff) {
            uint16_t sampleFormat;
            TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
            switch (sampleFormat)
            {
                case SAMPLEFORMAT_UINT:
                    return SampleFormat::UInt;
                case SAMPLEFORMAT_INT:
                    return SampleFormat::Int;
                case SAMPLEFORMAT_IEEEFP:
                    return SampleFormat::Float;
                default:
                    throw std::runtime_error("TIFF: unsupported sample format (void/complex)");
            }
        }

        struct AlphaInfo {
            bool hasAssocAlpha = false;
            bool hasUnassAlpha = false;
        };

        inline AlphaInfo getAlphaInfo(TIFF *tiff) {
            uint16_t  nExtra     = 0;
            uint16_t* extraTypes = nullptr;
            TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &nExtra, &extraTypes);

            bool hasAssocAlpha = false; // pre-multiplied (EXTRASAMPLE_ASSOCALPHA)
            bool hasUnassAlpha = false; // straight       (EXTRASAMPLE_UNASSALPHA)
            for (uint16_t i = 0; i < nExtra; ++i) {
                if (extraTypes[i] == EXTRASAMPLE_ASSOCALPHA) hasAssocAlpha = true;
                if (extraTypes[i] == EXTRASAMPLE_UNASSALPHA)  hasUnassAlpha  = true;
            }
            
            return { hasAssocAlpha, hasUnassAlpha };
        }

        inline ColorSpace getColorSpace(uint16_t photo, bool hasAlpha) {
            switch (photo) {
                case PHOTOMETRIC_MINISBLACK:
                case PHOTOMETRIC_MINISWHITE:
                    return hasAlpha ? ColorSpace::GrayA : ColorSpace::Gray;
                case PHOTOMETRIC_RGB:
                    return hasAlpha ? ColorSpace::RGBA : ColorSpace::RGB;
                case PHOTOMETRIC_SEPARATED: // CMYK
                    return hasAlpha ? ColorSpace::CMYKA : ColorSpace::CMYK;
                case PHOTOMETRIC_CIELAB:
                case PHOTOMETRIC_ICCLAB:
                case PHOTOMETRIC_ITULAB:
                    throw std::runtime_error("TIFF: LAB format is not supported in this version of the library");
                default:
                    return ColorSpace::Unknown;
            }
        }

        inline void readYCbCr(TIFF *tiff, TIFFData &bm) {
            bm.colorSpace      = ColorSpace::RGBA;
            bm.samplesPerPixel = 4;
            bm.bitsPerSample   = 8;
            bm.sampleFormat    = SampleFormat::UInt;

            const size_t npixels = static_cast<size_t>(bm.width) * bm.height;
            bm.dataSize = npixels * 4;
            bm.data     = new uint8_t[bm.dataSize];

            char emsg[1024] = {};
            TIFFRGBAImage img;
            if (!TIFFRGBAImageBegin(&img, tiff, 0, emsg))
                throw std::runtime_error(std::string("TIFF YCbCr error: ") + emsg);

            std::vector<uint32_t> raster(npixels);
            bool ok = TIFFRGBAImageGet(&img, raster.data(), bm.width, bm.height);
            TIFFRGBAImageEnd(&img);
            if (!ok)
                throw std::runtime_error("TIFF: TIFFRGBAImageGet failed for YCbCr");

            // TIFFRGBAImage writes rows bottom-to-top — flip to top-to-bottom
            for (uint32_t row = 0; row < bm.height; ++row) {
                uint8_t*  dst = bm.data + row * bm.width * 4;
                uint32_t* src = raster.data() + (bm.height - 1 - row) * bm.width;
                for (uint32_t x = 0; x < bm.width; ++x) {
                    dst[x*4+0] = TIFFGetR(src[x]);
                    dst[x*4+1] = TIFFGetG(src[x]);
                    dst[x*4+2] = TIFFGetB(src[x]);
                    dst[x*4+3] = TIFFGetA(src[x]);
                }
            }
        }
    
        inline void readTiles(TIFF *tiff, TIFFData &bm, bool isSeparate, size_t bytesPerPixel, size_t bytesPerSample) {
            uint32_t tileW = 0, tileH = 0;
            TIFFGetField(tiff, TIFFTAG_TILEWIDTH,  &tileW);
            TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tileH);

            if (!isSeparate) {
                std::vector<uint8_t> tileBuf(TIFFTileSize(tiff));
                for (uint32_t ty = 0; ty < bm.height; ty += tileH) {
                    for (uint32_t tx = 0; tx < bm.width; tx += tileW) {
                        if (TIFFReadTile(tiff, tileBuf.data(), tx, ty, 0, 0) < 0)
                            throw std::runtime_error("TIFF: failed to read tile");

                        uint32_t copyW = std::min(tileW, bm.width - tx);
                        uint32_t copyH = std::min(tileH, bm.height - ty);
                        for (uint32_t row = 0; row < copyH; ++row) {
                            uint8_t* src = tileBuf.data() + row * tileW * bytesPerPixel;
                            uint8_t* dst = bm.data + (ty + row) * bm.width * bytesPerPixel
                                                    + tx * bytesPerPixel;
                            std::memcpy(dst, src, copyW * bytesPerPixel);
                        }
                    }
                }
            } else {
                std::vector<uint8_t> tileBuf(TIFFTileSize(tiff));
                for (uint16_t s = 0; s < bm.samplesPerPixel; ++s) {
                    for (uint32_t ty = 0; ty < bm.height; ty += tileH) {
                        for (uint32_t tx = 0; tx < bm.width; tx += tileW) {
                            if (TIFFReadTile(tiff, tileBuf.data(), tx, ty, 0, s) < 0)
                                throw std::runtime_error("TIFF: failed to read planar tile");

                            uint32_t copyW = std::min(tileW, bm.width - tx);
                            uint32_t copyH = std::min(tileH, bm.height - ty);
                            for (uint32_t row = 0; row < copyH; ++row) {
                                for (uint32_t x = 0; x < copyW; ++x) {
                                    uint8_t* src = tileBuf.data()
                                                + (row * tileW + x) * bytesPerSample;
                                    uint8_t* dst = bm.data
                                                + ((ty + row) * bm.width + (tx + x)) * bytesPerPixel
                                                + s * bytesPerSample;
                                    std::memcpy(dst, src, bytesPerSample);
                                }
                            }
                        }
                    }
                }
            }
        }
    
        inline void readStrip(TIFF *tiff, TIFFData &bm, bool isSeparate, size_t bytesPerPixel, size_t bytesPerSample) {
            if (!isSeparate) {
                for (uint32_t row = 0; row < bm.height; ++row) {
                    uint8_t* dst = bm.data + row * bm.width * bytesPerPixel;
                    if (TIFFReadScanline(tiff, dst, row) < 0)
                        throw std::runtime_error("TIFF: failed to read scanline");
                }
            } else {
                std::vector<uint8_t> rowBuf(bm.width * bytesPerSample);
                for (uint16_t s = 0; s < bm.samplesPerPixel; ++s) {
                    for (uint32_t row = 0; row < bm.height; ++row) {
                        if (TIFFReadScanline(tiff, rowBuf.data(), row, s) < 0)
                            throw std::runtime_error("TIFF: failed to read planar scanline");
                        for (uint32_t x = 0; x < bm.width; ++x) {
                            uint8_t* src = rowBuf.data() + x * bytesPerSample;
                            uint8_t* dst = bm.data + (row * bm.width + x) * bytesPerPixel
                                                + s * bytesPerSample;
                            std::memcpy(dst, src, bytesPerSample);
                        }
                    }
                }
            }
        }
    
        inline void unPremultiplyInt(uint8_t* data, size_t numPixels, uint8_t spp, uint8_t bps) noexcept {
            const size_t bytesPerSample = bps / 8;
            const size_t bytesPerPixel  = spp * bytesPerSample;
            const int    alphaIdx       = spp - 1;
            const uint64_t maxVal       = (1ULL << bps) - 1;

            for (size_t p = 0; p < numPixels; ++p) {
                uint8_t* px = data + p * bytesPerPixel;

                uint64_t alpha = 0;
                std::memcpy(&alpha, px + alphaIdx * bytesPerSample, bytesPerSample);
                if (alpha == 0 || alpha == maxVal) continue;

                for (int s = 0; s < alphaIdx; ++s) {
                    uint64_t sample = 0;
                    std::memcpy(&sample, px + s * bytesPerSample, bytesPerSample);
                    uint64_t result = std::min((sample * maxVal + alpha / 2) / alpha, maxVal);
                    std::memcpy(px + s * bytesPerSample, &result, bytesPerSample);
                }
            }
        }

        inline void unPremultiplyFloat32(uint8_t* data, size_t numPixels, uint8_t spp) noexcept {
            float* pixels = reinterpret_cast<float*>(data);
            int    alphaIdx = spp - 1;
            for (size_t p = 0; p < numPixels; ++p) {
                float* px = pixels + p * spp;
                float  a  = px[alphaIdx];
                if (a > 0.0f && a < 1.0f)
                    for (int s = 0; s < alphaIdx; ++s) px[s] /= a;
            }
        }
    
        inline uint32_t readPackedSample(TIFFData &src, size_t index) noexcept {
            uint32_t result = 0;

            size_t bitOffset = index * src.bitsPerSample;

            for (uint8_t i = 0; i < src.bitsPerSample; i++) {
                size_t  byteIdx = (bitOffset + i) / 8;
                uint8_t bitIdx  = 7u - static_cast<uint8_t>((bitOffset + i) % 8); // MSB2LSB
                result = (result << 1u) | ((src.data[byteIdx] >> bitIdx) & 1u);
            }
            return result;
        }
        inline uint32_t bitReplicate(uint32_t val, uint8_t srcBits, uint8_t dstBits) noexcept {
            if (srcBits == dstBits) return val;
            uint32_t result = 0;
            int remaining = dstBits;
            while (remaining > 0) {
                int take = std::min((int)srcBits, remaining);
                result |= (val >> (srcBits - take)) << (remaining - take);
                remaining -= take;
            }
            return result;
        }
        inline void writeSample(void *outputData, uint32_t value, uint8_t targetBits, size_t index) noexcept {
            if (targetBits == 8) {
                memcpy((uint8_t*)outputData + index, &value, sizeof(uint8_t));
            } else if (targetBits == 16) {
                memcpy((uint16_t*)outputData + index, &value, sizeof(uint16_t));
            } else if (targetBits == 32) {
                memcpy((uint32_t*)outputData + index, &value, sizeof(uint32_t));
            }
        }
        inline void normalizeDepth(TIFFData &src) noexcept {
            const uint8_t bps = src.bitsPerSample;
            if (bps == 8 || bps == 16 || bps == 32) return;
            
            uint8_t targetBps = 8;
            if (bps < 8) targetBps = 8;
            else if (bps < 16) targetBps = 16;
            else targetBps = 32;

            const size_t totalSamples = src.width * src.height * src.samplesPerPixel;
            const size_t outputSize = totalSamples * (targetBps / 8);
            auto outputData = new uint8_t[outputSize];

            for (size_t i = 0; i < totalSamples; i++) {
                uint32_t val = readPackedSample(src, i);
                uint32_t normalized = bitReplicate(val, bps, targetBps);
                writeSample(outputData, normalized, targetBps, i);
            }

            delete [] src.data;
            src.data = outputData;
            src.dataSize = outputSize;
            src.bitsPerSample = targetBps;
        }
        
        inline void convertIntToUInt(TIFFData &src) noexcept {
            if (src.sampleFormat != SampleFormat::Int) return;

            const size_t totalSamples = src.width * src.height * src.samplesPerPixel;

            if (src.bitsPerSample == 8) {
                for (size_t i = 0; i < totalSamples; i++) {
                    src.data[i] = (uint8_t)((int16_t)(src.data[i]) + 128);
                }
            } else if (src.bitsPerSample == 16) {
                for (size_t i = 0; i < totalSamples; i++) {
                    src.data[i] = (uint16_t)((int32_t)(src.data[i]) + 32768);
                }
            } else {
                for (size_t i = 0; i < totalSamples; i++) {
                    src.data[i] = (uint32_t)((int64_t)(src.data[i]) + 2147483648LL);
                }            
            }

            src.sampleFormat = SampleFormat::UInt;
        }
    
        inline ColorProfile retrieveICCProfile(TIFF* tiff, TIFFData &src) {
            uint32_t iccSize = 0;
            void*    icc = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &iccSize, &icc) && iccSize > 0 && icc) {
                return ColorProfile::FromMemory(icc, iccSize);
            } else {
                switch (src.colorSpace) {
                    case ColorSpace::CMYK:
                    case ColorSpace::CMYKA:
                        return ColorProfile::CMYK();
                    case ColorSpace::RGB:
                    case ColorSpace::RGBA:
                        return ColorProfile::sRGB();
                    case ColorSpace::Gray:
                    case ColorSpace::GrayA: 
                        return ColorProfile::Gray();
                    default:
                        throw std::runtime_error("TIFF: Color Space is not defined or not supported");
                }
            }
        }

        inline SampleType getSampleType(TIFFData &src) noexcept {
            if (src.sampleFormat == SampleFormat::Float) {
                return SampleType::F32;
            } else {
                if (src.bitsPerSample == 8)       return SampleType::U8;
                else if (src.bitsPerSample == 16) return SampleType::U16;
                else                              return SampleType::U32;
            }
        }
        inline ColorModel getColorModel(TIFFData &src) {
            switch (src.colorSpace) {
                case ColorSpace::RGB:   return ColorModel::RGB;
                case ColorSpace::RGBA:  return ColorModel::RGBA;
                case ColorSpace::Gray:  return ColorModel::GRAY;
                case ColorSpace::GrayA: return ColorModel::GRAYA;
                case ColorSpace::CMYK:  return ColorModel::CMYK;
                case ColorSpace::CMYKA: return ColorModel::CMYKA;
                default: throw std::runtime_error("TIFF: Color Model is not defined");
            }
        }

        inline Bitmap convertForSaving(const Bitmap &src) {
            if (src.colorModel == ColorModel::XYZ) {
                return src.convertColorModel(ColorModel::RGB);
            } else {
                return src.copy();
            }
        }

        struct SampleInfo {
            uint16_t bitsPerSample = 0;
            uint16_t sampleFormat  = 0;
        };

        inline SampleInfo getSampleInfo(const SampleType sampleType) {
            switch (sampleType) {
                case SampleType::U8:  return { 8,  SAMPLEFORMAT_UINT };
                case SampleType::U16: return { 16, SAMPLEFORMAT_UINT };
                case SampleType::U32: return { 32, SAMPLEFORMAT_UINT };
                case SampleType::F32: return { 32, SAMPLEFORMAT_IEEEFP };
                default: return { 0, 0 };
            }
        }

        struct ColorInfo {
            uint16_t samplesPerPixel    = 0;
            uint16_t photometric        = 0;
            uint16_t extraSamplesCount  = 0;
            uint16_t extraSampleType    = EXTRASAMPLE_UNASSALPHA;
        };

        inline ColorInfo getColorInfo(const ColorModel colorModel) {
            switch (colorModel) {
                case ColorModel::RGB:   return { 3, PHOTOMETRIC_RGB, 0 };
                case ColorModel::RGBA:  return { 4, PHOTOMETRIC_RGB, 1 };
                case ColorModel::GRAY:  return { 1, PHOTOMETRIC_MINISBLACK, 0 };
                case ColorModel::GRAYA: return { 2, PHOTOMETRIC_MINISBLACK, 1 };
                case ColorModel::CMYK:  return { 4, PHOTOMETRIC_SEPARATED, 0 };
                case ColorModel::CMYKA: return { 5, PHOTOMETRIC_SEPARATED, 1 };
                default: return { 0, 0, 0 };
            }
        }
    }

    Bitmap loadTIFF(const char* filename) {
        TIFF* tiff = TIFFOpen(filename, "r");
        if (!tiff) {
            throw std::runtime_error("Failed to open file");
        }

        struct Guard { TIFF* t; ~Guard() { TIFFClose(t); } } guard{tiff};
        
        TIFFData tiffData {};

        uint32_t width, height;
        if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH,  &width) ||
            !TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height))
            throw std::runtime_error("TIFF: missing width or height tag");
        tiffData.width  = width;
        tiffData.height = height;

        uint16_t spp = 1;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &spp);
        tiffData.samplesPerPixel = static_cast<uint8_t>(spp);

        uint16_t bps = 1;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &bps);
        tiffData.bitsPerSample = static_cast<uint8_t>(bps);
        if (bps > 32) {
            throw std::runtime_error("TIFF: Images with more than 32 bits per sample are not supported in this version of the library");
        }

        tiffData.sampleFormat = getSampleFormat(tiff);
        if (tiffData.sampleFormat == SampleFormat::Float && bps != 32) {
            throw std::runtime_error("TIFF: Float16 is not supported in this version of the library");
        }
        AlphaInfo alphaInfo = getAlphaInfo(tiff);

        uint16_t photo = PHOTOMETRIC_RGB;
        if (!TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photo)) {
            throw std::runtime_error("TIFF: Photometric tas is not defined");
        }

        const bool isPalette    = (photo == PHOTOMETRIC_PALETTE);
        const bool isYCbCr      = (photo == PHOTOMETRIC_YCBCR);
        const bool isMinIsWhite = (photo == PHOTOMETRIC_MINISWHITE);

        bool hasAlpha = alphaInfo.hasAssocAlpha || alphaInfo.hasUnassAlpha;

        if (!isPalette && !isYCbCr) {
            tiffData.colorSpace = getColorSpace(photo, hasAlpha);
        }

        if (isPalette) {
            // TODO: add palette support
            throw std::runtime_error("TIFF: palette color format is not supported in this version of the library");
        }
        else if (isYCbCr) {
            readYCbCr(tiff, tiffData);
        } else {
            if (tiffData.colorSpace == ColorSpace::Unknown) {
                throw std::runtime_error("TIFF: File does not define image color space or format is not supported");
            }

            uint16_t planarConfig = PLANARCONFIG_CONTIG;
            TIFFGetFieldDefaulted(tiff, TIFFTAG_PLANARCONFIG, &planarConfig);
            const bool isSeparate = (planarConfig == PLANARCONFIG_SEPARATE);

            const size_t bytesPerSample = (bps + 7) / 8;
            const size_t bytesPerPixel  = spp * bytesPerSample;
            tiffData.dataSize = static_cast<size_t>(tiffData.width) * tiffData.height * bytesPerPixel;
            tiffData.data     = new uint8_t[tiffData.dataSize];

            if (TIFFIsTiled(tiff)) {
                readTiles(tiff, tiffData, isSeparate, bytesPerPixel, bytesPerSample);
            } else {
                readStrip(tiff, tiffData, isSeparate, bytesPerPixel, bytesPerSample);
            }

            if (isMinIsWhite) {
                if (tiffData.sampleFormat == SampleFormat::Float) {
                    throw std::runtime_error("TIFF: tag PHOTOMETRIC_MINISWHITE does not support float sample format");
                }
                const uint64_t maxVal = (1ULL << bps) - 1;
                for (size_t i = 0; i < tiffData.dataSize / bytesPerSample; ++i) {
                    uint64_t v = 0;
                    std::memcpy(&v, tiffData.data + i * bytesPerSample, bytesPerSample);
                    v = maxVal - v;
                    std::memcpy(tiffData.data + i * bytesPerSample, &v, bytesPerSample);
                }
            }
            if (alphaInfo.hasAssocAlpha) {
                const size_t numPixels = static_cast<size_t>(tiffData.width) * tiffData.height;
                if (tiffData.sampleFormat == SampleFormat::Float) {
                    if (bps == 32) unPremultiplyFloat32(tiffData.data, numPixels, spp);
                    else throw std::runtime_error("TIFF: only Float32 is supported in this version of the library");
                } else {
                    unPremultiplyInt(tiffData.data, numPixels, spp, bps);
                }
            }
        }

        normalizeDepth(tiffData);
        convertIntToUInt(tiffData);

        auto profile = retrieveICCProfile(tiff, tiffData);

        SampleType sampleType = getSampleType(tiffData);
        ColorModel colorModel = getColorModel(tiffData);

        uint8_t* buffer = tiffData.data;
        tiffData.data = nullptr;

        return Bitmap(
            tiffData.width, tiffData.height, buffer, 
            sampleType, colorModel, profile
        );
    }

    void saveTIFF(const char* filename, const Bitmap &bitmap, Properties props) {
        TIFF* tiff = TIFFOpen(filename, "w");
        if (!tiff) {
            throw std::runtime_error("TIFF: cannot open file: " + std::string(filename));
        }

        Bitmap src = convertForSaving(bitmap);

        const SampleInfo sampleInfo = getSampleInfo(src.sampleType);
        if (sampleInfo.bitsPerSample == 0 || sampleInfo.sampleFormat == 0) {
            TIFFClose(tiff);
            throw std::runtime_error("TIFF: unsupported SampleType");
        }

        const ColorInfo colorInfo = getColorInfo(src.colorModel);
        if (colorInfo.samplesPerPixel == 0) {
            TIFFClose(tiff);
            throw std::runtime_error("TIFF: unsupported ColorModel");
        }

        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH,      src.width);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH,     src.height);
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE,   sampleInfo.bitsPerSample);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, colorInfo.samplesPerPixel);
        TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT,    sampleInfo.sampleFormat);
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC,     colorInfo.photometric);
        TIFFSetField(tiff, TIFFTAG_ORIENTATION,     ORIENTATION_TOPLEFT);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
        TIFFSetField(tiff, TIFFTAG_COMPRESSION,     COMPRESSION_NONE);

        if (colorInfo.extraSamplesCount > 0) {
            TIFFSetField(
                tiff, TIFFTAG_EXTRASAMPLES, 
                colorInfo.extraSamplesCount, 
                &colorInfo.extraSampleType
            );
        }

        auto icc = bitmap.profile.toICC();
        TIFFSetField(
            tiff, TIFFTAG_ICCPROFILE, 
            static_cast<uint32_t>(icc.size()), icc.data()
        );

        size_t   rowStep = static_cast<size_t>(bitmap.stride);

        for (uint32_t y = 0; y < bitmap.height; ++y) {
            uint8_t* row = src.buffer + y * rowStep;

            if (TIFFWriteScanline(tiff, row, y, 0) < 0) {
                TIFFClose(tiff);
                throw std::runtime_error("TIFF: TIFFWriteScanline failed");
            }
        }

        TIFFClose(tiff);
    }
}
