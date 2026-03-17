#include <stdio.h>
#include "image-io.h"

int main() {
    const char* filename = "D:\\1.tif";

    ImageIO::Bitmap bitmap = ImageIO::open(filename, ImageIO::SampleType::U16, ImageIO::ColorSpace::sRGB_Linear());

    ImageIO::Properties props{};
    props.jpegQuality = 10;

    ImageIO::save("D:\\0.jpg", bitmap);
    // ImageIO::save("D:\\ProPhoto -.tiff", *bitmap);

    return 0;
}