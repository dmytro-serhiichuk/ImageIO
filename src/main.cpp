#include <stdio.h>
#include "image-io.h"
#include "profile-management.h"

int main() {
    const char* filename = "D:\\2.jpg";

    // uint8_t pixel[3] = { 54, 54, 54 };
    // uint8_t res[3];

    // ImageIO::ColorSpace inColor = { ImageIO::Channels::RGB, ImageIO::ColorGamut::sRGB, ImageIO::TransferFunction::Linear };
    // ImageIO::ColorSpace outColor = { ImageIO::Channels::RGB, ImageIO::ColorGamut::sRGB, ImageIO::TransferFunction::sRGB };

    // auto inType = inColor.buildLcmsType(1, false);
    // auto outType = outColor.buildLcmsType(1, false);

    // auto inProfile = inColor.createProfile();
    // auto outProfile = outColor.createProfile();

    // cmsHTRANSFORM t = cmsCreateTransform(
    //     inProfile, inType,
    //     outProfile, outType,
    //     INTENT_RELATIVE_COLORIMETRIC, 0
    // );

    // cmsDoTransform(t, pixel, res, 1);

    auto AdobeRGB = ImageIO::createAdobeRGBProfile();
    auto ProPhotoRGB = ImageIO::createProPhotoProfile();
    auto WideGamut = ImageIO::createWideGamutProfile();

    ImageIO::Bitmap bitmap = ImageIO::open(
        filename, 
        ImageIO::SampleType::U16,
        ImageIO::ColorModel::RGB,
        WideGamut
    );

    ImageIO::Properties props{};
    props.jpegQuality = 100;

    ImageIO::save("D:\\sRGBA.jpg", bitmap, props);
    // ImageIO::save("D:\\sRGBA.png", bitmap, props);
    ImageIO::save("D:\\sRGBA.tiff", bitmap, props);
    // ImageIO::save("D:\\0.png", bitmap, props);
    // ImageIO::save("D:\\0.tiff", bitmap, props);

    return 0;
}