#include "tiff-io.h"
#include <tiffio.h>
#include <lcms2.h>

namespace ImageIO {
    namespace {
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

        inline void writeProfileToMem(cmsHPROFILE profile, uint8_t*& icc, uint32_t &iccSize) {
            cmsSaveProfileToMem(profile, NULL, &iccSize);
            icc = new uint8_t[iccSize]();
            cmsSaveProfileToMem(profile, icc, &iccSize);
        } 

        inline void retrieveICCProfile(TIFF* tiff, uint8_t *&icc, uint32_t &iccSize) {
            uint32_t _iccSize = 0;
            void*    _icc = nullptr;
            if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &_iccSize, &_icc)
                && _iccSize > 0 && _icc)
            {
                iccSize = _iccSize;
                icc     = new uint8_t[iccSize];
                std::memcpy(icc, _icc, iccSize);
            }
        }

        inline void readYCbCr(TIFF *tiff, NativeBitmap &bm) {
            bm.colorSpace      = NativeColorSpace::RGBA;
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

        inline void readTiles(TIFF *tiff, NativeBitmap &bm, bool isSeparate, size_t bytesPerPixel, size_t bytesPerSample) {
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
    
        inline void readStrip(TIFF *tiff, NativeBitmap &bm, bool isSeparate, size_t bytesPerPixel, size_t bytesPerSample) {
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
    
        inline void unPremultiplyInt(uint8_t* data, size_t numPixels, uint8_t spp, uint8_t bps) {
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

        inline void unPremultiplyFloat32(uint8_t* data, size_t numPixels, uint8_t spp) {
            float* pixels = reinterpret_cast<float*>(data);
            int    alphaIdx = spp - 1;
            for (size_t p = 0; p < numPixels; ++p) {
                float* px = pixels + p * spp;
                float  a  = px[alphaIdx];
                if (a > 0.0f && a < 1.0f)
                    for (int s = 0; s < alphaIdx; ++s) px[s] /= a;
            }
        }

        inline void unPremultiplyFloat64(uint8_t* data, size_t numPixels, uint8_t spp) {
            double* pixels = reinterpret_cast<double*>(data);
            int     alphaIdx = spp - 1;
            for (size_t p = 0; p < numPixels; ++p) {
                double* px = pixels + p * spp;
                double  a  = px[alphaIdx];
                if (a > 0.0 && a < 1.0)
                    for (int s = 0; s < alphaIdx; ++s) px[s] /= a;
            }
        }
    
        struct SampleInfo {
            uint16_t bitsPerSample = 0;
            uint16_t sampleFormat  = 0;
        };

        inline SampleInfo getSampleInfo(const SampleType sampleType) {
            switch (sampleType) {
                case SampleType::U8:  return { 8, SAMPLEFORMAT_UINT };
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

        inline ColorInfo getColorInfo(const Channels channels) {
            switch (channels) {
                case Channels::RGB:            return { 3, PHOTOMETRIC_RGB, 0 };
                case Channels::RGBA:           return { 4, PHOTOMETRIC_RGB, 1 };
                case Channels::Grayscale:      return { 1, PHOTOMETRIC_MINISBLACK, 0 };
                case Channels::GrayscaleAlpha: return { 2, PHOTOMETRIC_MINISBLACK, 1 };
                default: return { 0, 0, 0 };
            }
        }
    }

    NativeBitmap loadTIFF(const char* filename) {
        TIFF* tiff = TIFFOpen(filename, "r");
        if (!tiff) {
            throw std::runtime_error("Failed to open file");
        }

        struct Guard { TIFF* t; ~Guard() { TIFFClose(t); } } guard{tiff};

        NativeBitmap bm {};

        uint32_t width, height;
        if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH,  &width) ||
            !TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height))
            throw std::runtime_error("TIFF: missing width or height tag");
        bm.width  = width;
        bm.height = height;

        uint16_t spp = 1;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &spp);
        bm.samplesPerPixel = static_cast<uint8_t>(spp);

        uint16_t bps = 1;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &bps);
        bm.bitsPerSample = static_cast<uint8_t>(bps);
        if (bps > 64) {
            throw std::runtime_error("TIFF: Images with more than 64 bits per sample are not supported in this version of the library");
        }

        bm.sampleFormat = getSampleFormat(tiff);
        if (bm.sampleFormat == SampleFormat::Float && !(bps == 32 || bps == 64)) {
            throw std::runtime_error("TIFF: Float16 is not supported in this version of the library");
        }
        AlphaInfo alphaInfo = getAlphaInfo(tiff);

        uint16_t photo = PHOTOMETRIC_RGB;
        TIFFGetFieldDefaulted(tiff, TIFFTAG_PHOTOMETRIC, &photo);

        const bool isPalette    = (photo == PHOTOMETRIC_PALETTE);
        const bool isYCbCr      = (photo == PHOTOMETRIC_YCBCR);
        const bool isMinIsWhite = (photo == PHOTOMETRIC_MINISWHITE);

        bool hasAlpha = alphaInfo.hasAssocAlpha || alphaInfo.hasUnassAlpha;
        bool isLAB = false;

        if (!isPalette && !isYCbCr) {
            switch (photo) {
                case PHOTOMETRIC_MINISBLACK:
                case PHOTOMETRIC_MINISWHITE:
                    bm.colorSpace = hasAlpha ? NativeColorSpace::GrayscaleAlpha
                                             : NativeColorSpace::Grayscale;
                    break;
                case PHOTOMETRIC_RGB:
                    bm.colorSpace = hasAlpha ? NativeColorSpace::RGBA
                                             : NativeColorSpace::RGB;
                    break;
                case PHOTOMETRIC_SEPARATED: // CMYK
                    bm.colorSpace = hasAlpha ? NativeColorSpace::CMYKA
                                             : NativeColorSpace::CMYK;
                    break;
                case PHOTOMETRIC_CIELAB:
                case PHOTOMETRIC_ICCLAB:
                case PHOTOMETRIC_ITULAB:
                    isLAB = true;
                    break;
                default:
                    bm.colorSpace = NativeColorSpace::Unknown;
                    break;
            }
        }

        uint8_t *icc = nullptr;
        uint32_t iccSize = 0;
        retrieveICCProfile(tiff, icc, iccSize);
        bm.iccProfile = icc;
        bm.iccProfileSize = iccSize;

        if (bm.iccProfile == nullptr || bm.iccProfileSize == 0) {
            if (isLAB) {
                throw std::runtime_error("TIFF: LAB ICC Profile is not defined");
            }

            switch (bm.colorSpace) {
                case NativeColorSpace::CMYK:
                case NativeColorSpace::CMYKA:
                    // TODO: create CMYK icc profile
                    break;
                case NativeColorSpace::Grayscale:
                case NativeColorSpace::GrayscaleAlpha: {
                    auto curve = cmsBuildGamma(nullptr, 2.2);
                    auto profile = cmsCreateGrayProfile(cmsD50_xyY(), curve);
                    cmsFreeToneCurve(curve);
                    writeProfileToMem(profile, bm.iccProfile, bm.iccProfileSize);
                    cmsCloseProfile(profile);
                    break;
                } 
                default: { // rgb
                    auto profile = cmsCreate_sRGBProfile();
                    writeProfileToMem(profile, bm.iccProfile, bm.iccProfileSize);
                    cmsCloseProfile(profile);
                    break;
                }
            }
        } else if (isLAB) {
            auto profile = cmsOpenProfileFromMem(bm.iccProfile, bm.iccProfileSize);
            auto ver = cmsGetProfileVersion(profile);
            if (ver < 4.0) {
                bm.colorSpace = hasAlpha ? NativeColorSpace::ALAB : NativeColorSpace::LAB;
            } else {
                bm.colorSpace = hasAlpha ? NativeColorSpace::ALAB2 : NativeColorSpace::LAB2;
            }
        }

        if (isPalette) {
            // TODO: add palette support
            throw std::runtime_error("TIFF: palette color format is not supported in this version of the library");
        }
        else if (isYCbCr) {
            readYCbCr(tiff, bm);
        } else {
            if (bm.colorSpace == NativeColorSpace::Unknown) {
                throw std::runtime_error("TIFF: File does not define image color space or format is not supported");
            }

            uint16_t planarConfig = PLANARCONFIG_CONTIG;
            TIFFGetFieldDefaulted(tiff, TIFFTAG_PLANARCONFIG, &planarConfig);
            const bool isSeparate = (planarConfig == PLANARCONFIG_SEPARATE);

            const size_t bytesPerSample = (bps + 7) / 8;
            const size_t bytesPerPixel  = spp * bytesPerSample;
            bm.dataSize = static_cast<size_t>(bm.width) * bm.height * bytesPerPixel;
            bm.data     = new uint8_t[bm.dataSize];

            if (TIFFIsTiled(tiff)) {
                readTiles(tiff, bm, isSeparate, bytesPerPixel, bytesPerSample);
            } else {
                readStrip(tiff, bm, isSeparate, bytesPerPixel, bytesPerSample);
            }

            if (isMinIsWhite) {
                if (bm.sampleFormat == SampleFormat::Float) {
                    throw std::runtime_error("TIFF: tag PHOTOMETRIC_MINISWHITE does not support float sample format");
                }
                const uint64_t maxVal = (1ULL << bps) - 1;
                for (size_t i = 0; i < bm.dataSize / bytesPerSample; ++i) {
                    uint64_t v = 0;
                    std::memcpy(&v, bm.data + i * bytesPerSample, bytesPerSample);
                    v = maxVal - v;
                    std::memcpy(bm.data + i * bytesPerSample, &v, bytesPerSample);
                }
            }
            if (alphaInfo.hasAssocAlpha) {
                const size_t numPixels = static_cast<size_t>(bm.width) * bm.height;
                if (bm.sampleFormat == SampleFormat::Float) {
                    if (bps == 16)      throw std::runtime_error("TIFF: Float16 is not supported in this version of the library");
                    else if (bps == 32) unPremultiplyFloat32(bm.data, numPixels, spp);
                    else if (bps == 64) unPremultiplyFloat64(bm.data, numPixels, spp);
                } else {
                    unPremultiplyInt(bm.data, numPixels, spp, bps);
                }
            }
        }

        return bm;
    }

    void saveTIFF(const char* filename, const Bitmap &bitmap, Properties props) {
        TIFF* tiff = TIFFOpen(filename, "w");
        if (!tiff) {
            throw std::runtime_error("TIFF: cannot open file: " + std::string(filename));
        }

        const SampleInfo sampleInfo = getSampleInfo(bitmap.sampleType);
        if (sampleInfo.bitsPerSample == 0 || sampleInfo.sampleFormat == 0) {
            TIFFClose(tiff);
            throw std::runtime_error("TIFF: unsupported SampleType");
        }

        const ColorInfo colorInfo = getColorInfo(bitmap.colorSpace.channels);
        if (colorInfo.samplesPerPixel == 0) {
            TIFFClose(tiff);
            throw std::runtime_error("TIFF: unsupported channel layout");
        }

        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH,      bitmap.width);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH,     bitmap.height);
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

        auto icc = bitmap.getICCProfile();
        TIFFSetField(
            tiff, TIFFTAG_ICCPROFILE, 
            static_cast<uint32_t>(icc.size()), icc.data()
        );

        uint8_t* bufPtr  = bitmap.ptr<uint8_t>();
        size_t   rowStep = static_cast<size_t>(bitmap.stride);

        for (uint32_t y = 0; y < bitmap.height; ++y) {
            uint8_t* row = bufPtr + y * rowStep;

            if (TIFFWriteScanline(tiff, row, y, 0) < 0) {
                TIFFClose(tiff);
                throw std::runtime_error("TIFF: TIFFWriteScanline failed");
            }
        }

        TIFFClose(tiff);
    }
}
