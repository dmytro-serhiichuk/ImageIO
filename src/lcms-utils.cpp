#include "lcms-utils.h"

namespace ImageIO {
    cmsUInt32Number buildLcmsType(ColorModel colorModel, SampleType sampleType)
    {
        cmsUInt32Number colorSpaceFlag = 0;
        cmsUInt32Number channelsCount = 0;
        cmsUInt32Number extraChannels = 0;

        auto channels = getSamplesPerPixel(colorModel);
        auto bps = getBytesPerSample(sampleType);
        auto isFloat = sampleType == SampleType::F32;

        switch (colorModel) {
            case ColorModel::RGB:
            case ColorModel::RGBA:
                colorSpaceFlag = PT_RGB;
                channelsCount = 3;
                extraChannels = channels - 3;
                break;
            case ColorModel::CMYK:
            case ColorModel::CMYKA:
                colorSpaceFlag = PT_CMYK;
                channelsCount = 4;
                extraChannels = channels - 4;
                break;
            case ColorModel::XYZ:
                colorSpaceFlag = PT_XYZ;
                channelsCount = 3;
                extraChannels = 0;
                break;
            default: // gray
                colorSpaceFlag = PT_GRAY;
                channelsCount = 1;
                extraChannels = channels - 1;
                break;
        }
        
        return (COLORSPACE_SH(colorSpaceFlag) |
                CHANNELS_SH(channelsCount) |
                BYTES_SH((cmsUInt32Number)bps) |
                FLOAT_SH((cmsUInt32Number)isFloat) |
                EXTRA_SH(extraChannels));
    }

    namespace {
        cmsBool CompareXYZ(cmsHPROFILE h1, cmsHPROFILE h2, cmsTagSignature tag) {
            cmsCIEXYZ *a = (cmsCIEXYZ*)cmsReadTag(h1, tag);
            cmsCIEXYZ *b = (cmsCIEXYZ*)cmsReadTag(h2, tag);
            if (!a || !b) return FALSE;

            double eps = 1e-4;
            return fabs(a->X - b->X) < eps &&
                fabs(a->Y - b->Y) < eps &&
                fabs(a->Z - b->Z) < eps;
        }

        cmsBool AreSameMatrixProfiles(cmsHPROFILE h1, cmsHPROFILE h2) {
            if (cmsGetColorSpace(h1)    != cmsGetColorSpace(h2))    return FALSE;
            if (cmsGetDeviceClass(h1)   != cmsGetDeviceClass(h2))   return FALSE;

            if (!CompareXYZ(h1, h2, cmsSigRedColorantTag))          return FALSE;
            if (!CompareXYZ(h1, h2, cmsSigGreenColorantTag))        return FALSE;
            if (!CompareXYZ(h1, h2, cmsSigBlueColorantTag))         return FALSE;
            if (!CompareXYZ(h1, h2, cmsSigMediaWhitePointTag))      return FALSE;

            cmsToneCurve *trc1, *trc2;
            cmsTagSignature trcs[] = {
                cmsSigRedTRCTag, cmsSigGreenTRCTag, cmsSigBlueTRCTag
            };

            for (int i = 0; i < 3; i++) {
                trc1 = (cmsToneCurve*)cmsReadTag(h1, trcs[i]);
                trc2 = (cmsToneCurve*)cmsReadTag(h2, trcs[i]);
                if (!trc1 || !trc2) return FALSE;
                
                int N = 256;
                for (int j = 0; j < N; j++) {
                    cmsFloat32Number x = (cmsFloat32Number)j / (N - 1);
                    cmsFloat32Number y1 = cmsEvalToneCurveFloat(trc1, x);
                    cmsFloat32Number y2 = cmsEvalToneCurveFloat(trc2, x);
                    if (fabs(y1 - y2) > 1e-4f) return FALSE;
                }
            }

            return TRUE;
        }
    }

    bool profileEqual(cmsHPROFILE p1, cmsHPROFILE p2) {
        if (!p1 || !p2) return true;
        cmsUInt8Number digest1[16], digest2[16];

        cmsMD5computeID(p1);
        cmsMD5computeID(p2);

        cmsGetHeaderProfileID(p1, digest1);
        cmsGetHeaderProfileID(p2, digest2);
        
        return memcmp(digest1, digest2, 16) == 0 || AreSameMatrixProfiles(p1, p2);
    }
}