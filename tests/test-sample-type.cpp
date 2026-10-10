#include <gtest/gtest.h>
#include <ImageIO/image-io.h>


TEST(SampleTypeConversion, U8ToU16) {
    uint8_t* src = new uint8_t[3] { 0, 127, 255 };
    auto u8 = ImageIO::Bitmap(
        3, 1, src, ImageIO::SampleType::U8, 
        ImageIO::ColorModel::GRAY, ImageIO::ColorProfile::Gray()
    );

    auto u16 = u8.convertSampleType(ImageIO::SampleType::U16);
    auto res = (uint16_t*)u16.buffer;

    EXPECT_EQ(res[0], 0);
    EXPECT_EQ(res[1], 32639);
    EXPECT_EQ(res[2], 65535);
}

TEST(SampleTypeConversion, U8ToU32) {
    uint8_t* src = new uint8_t[3] { 0, 127, 255 };
    auto u8 = ImageIO::Bitmap(
        3u, 1u, src, ImageIO::SampleType::U8, 
        ImageIO::ColorModel::GRAY, ImageIO::ColorProfile::Gray()
    );

    auto u32 = u8.convertSampleType(ImageIO::SampleType::U32);
    auto res = (uint32_t*)u32.buffer;

    EXPECT_EQ(res[0], 0u);
    EXPECT_EQ(res[1], 2139062143u);
    EXPECT_EQ(res[2], 4294967295u);
}

TEST(SampleTypeConversion, U32ToU8) {
    uint32_t* src = new uint32_t[3] { 0u, 2139062143u, 4294967295u };
    auto u32 = ImageIO::Bitmap(
        3u, 1u, src, ImageIO::SampleType::U32, 
        ImageIO::ColorModel::GRAY, ImageIO::ColorProfile::Gray()
    );

    auto u8 = u32.convertSampleType(ImageIO::SampleType::U8);
    auto res = (uint8_t*)u8.buffer;

    EXPECT_EQ(res[0], 0);
    EXPECT_EQ(res[1], 127);
    EXPECT_EQ(res[2], 255);
}

TEST(SampleTypeConversion, F32ToU32) {
    float* src = new float[6] { 0.0f, 0.5f, 1.0f, -1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN() };
    auto f32 = ImageIO::Bitmap(
        6u, 1u, src, ImageIO::SampleType::F32, 
        ImageIO::ColorModel::GRAY, ImageIO::ColorProfile::Gray()
    );

    auto u32 = f32.convertSampleType(ImageIO::SampleType::U32);
    auto res = (uint32_t*)u32.buffer;

    EXPECT_EQ(res[0], 0u);
    EXPECT_EQ(res[1], 2147483647u);
    EXPECT_EQ(res[2], 4294967295u);
    EXPECT_EQ(res[3], 0u);
    EXPECT_EQ(res[4], 4294967295u);
    EXPECT_EQ(res[5], 0u);
}