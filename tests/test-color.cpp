#include <gtest/gtest.h>
#include <ImageIO/image-io.h>

class ColorTest : public ::testing::Test {
protected:
    void SetUp() override { 
        bitmap = ImageIO::open(IMAGEIO_TEST_DATA_DIR "/test-png-image.png");
    }
    void TearDown() override {}

    ImageIO::Bitmap bitmap {};
};


TEST_F(ColorTest, RgbModelWithSRGB) {
    ASSERT_NO_THROW(bitmap.convertColorModel(ImageIO::ColorModel::RGB, ImageIO::ColorProfile::sRGB()));
}

TEST_F(ColorTest, RgbModelWithAdobeRGB) {
    ASSERT_NO_THROW(bitmap.convertColorModel(ImageIO::ColorModel::RGB, ImageIO::ColorProfile::AdobeRGB()));
}

TEST_F(ColorTest, RgbModelWithProPhoto) {
    ASSERT_NO_THROW(bitmap.convertColorModel(ImageIO::ColorModel::RGB, ImageIO::ColorProfile::ProPhotoRGB()));
}

TEST_F(ColorTest, RgbModelWithNotRgbColorSpace) {
    ASSERT_ANY_THROW(bitmap.convertColorModel(ImageIO::ColorModel::RGB, ImageIO::ColorProfile::CMYK()));
}

TEST_F(ColorTest, CmykModelWithRgbColorSpace) {
    ASSERT_ANY_THROW(bitmap.convertColorModel(ImageIO::ColorModel::CMYK, ImageIO::ColorProfile::sRGB()));
}

TEST_F(ColorTest, CmykModelWithCmykSpace) {
    ASSERT_NO_THROW(bitmap.convertColorModel(ImageIO::ColorModel::CMYK, ImageIO::ColorProfile::CMYK()));
}

TEST_F(ColorTest, GrayModelWithGraySpace) {
    ASSERT_NO_THROW(bitmap.convertColorModel(ImageIO::ColorModel::GRAY, ImageIO::ColorProfile::Gray()));
}

TEST_F(ColorTest, GrayModelWithNotGraySpace) {
    ASSERT_ANY_THROW(bitmap.convertColorModel(ImageIO::ColorModel::GRAY, ImageIO::ColorProfile::AdobeRGB()));
}