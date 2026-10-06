#include <gtest/gtest.h>
#include <ImageIO/image-io.h>

class SampleTypeConversationTest : public ::testing::Test {
protected:
    void SetUp() override { 
        bitmap = ImageIO::open(IMAGEIO_TEST_DATA_DIR "/test-png-image.png");
        index = 7 * ImageIO::getSamplesPerPixel(bitmap.colorModel);
        val8 = bitmap.buffer[index];
    }
    void TearDown() override {}

    ImageIO::Bitmap bitmap {};
    size_t index;
    uint8_t val8;
};

TEST_F(SampleTypeConversationTest, U8ToU16Conversation) {
    auto bitmap2 = bitmap.convertSampleType(ImageIO::SampleType::U16);
    auto bitmap2Buffer = (uint16_t*)bitmap2.buffer;
    uint16_t val16 = bitmap2Buffer[index];

    ASSERT_EQ(val16 >> 8, val8);
}

TEST_F(SampleTypeConversationTest, U8ToU32Conversation) {
    auto bitmap2 = bitmap.convertSampleType(ImageIO::SampleType::U32);
    auto bitmap2Buffer = (uint32_t*)bitmap2.buffer;
    uint32_t val32 = bitmap2Buffer[index];

    ASSERT_EQ(val32 >> 24, val8);
}

TEST_F(SampleTypeConversationTest, U8ToF32Conversation) {
    auto bitmap2 = bitmap.convertSampleType(ImageIO::SampleType::F32);
    auto bitmap2Buffer = (float*)bitmap2.buffer;
    float valF = bitmap2Buffer[index];

    ASSERT_EQ(valF, static_cast<float>(val8) / 255.0F);
}