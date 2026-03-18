#include <stdio.h>
#include "image-io.h"

int main() {
    const char* filename = "D:\\1.CR2";

    ImageIO::Bitmap bitmap = ImageIO::open(filename, ImageIO::SampleType::U16, ImageIO::ColorSpace::ProPhoto());

    ImageIO::Properties props{};
    props.jpegQuality = 100;

    ImageIO::save("D:\\0.jpg", bitmap, props);
    ImageIO::save("D:\\0.png", bitmap, props);
    ImageIO::save("D:\\0.tiff", bitmap, props);
    // ImageIO::save("D:\\ProPhoto -.tiff", *bitmap);

    return 0;
}