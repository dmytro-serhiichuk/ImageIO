#include <gtest/gtest.h>
#include <ImageIO/image-io.h>

struct FormatTestCase {
    const char* fileName;
    ImageIO::Format expectedFormat;
};

class FormatTest : public ::testing::TestWithParam<FormatTestCase> {};

TEST_P(FormatTest, Reading) {
    const auto& params = GetParam();
    auto format = ImageIO::getFormat(params.fileName);
    ASSERT_EQ(format, params.expectedFormat);
}

INSTANTIATE_TEST_SUITE_P(
    FormatTests,
    FormatTest,
    ::testing::Values(
        FormatTestCase { IMAGEIO_TEST_DATA_DIR "/test-png-image.png", ImageIO::Format::PNG },
        FormatTestCase { IMAGEIO_TEST_DATA_DIR "/test-png-image-with-jpg-extension.jpg", ImageIO::Format::PNG },
        FormatTestCase { IMAGEIO_TEST_DATA_DIR "/test-fake-image.png", ImageIO::Format::UNDEFINED },
        FormatTestCase { IMAGEIO_TEST_DATA_DIR "/test-tiff-image.tif", ImageIO::Format::TIFF }
    )
);