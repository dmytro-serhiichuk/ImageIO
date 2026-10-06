#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include "ImageIO/image-io.h"
#include "jpeg-io.h"
#include "png-io.h"
#include "raw-i.h"
#include "tiff-io.h"

namespace ImageIO {
    namespace {
        std::string getFileExtension(const char *filename) {
            std::string str(filename);
            size_t dotPos = str.find_last_of(".");
            if (dotPos == std::string::npos) return "";

            std::string ext = str.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }
    }

    Format getFormat(const char *filename) {
        uint8_t buffer[8];
        std::ifstream in(filename, std::ios::binary);
        if (!in) return Format::UNDEFINED;
        in.read((char*)buffer, 8);
        if (in.gcount() < 8) return Format::UNDEFINED;

        if (buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF) {
            return Format::JPG;
        }

        if (buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47 &&
            buffer[4] == 0x0D && buffer[5] == 0x0A && buffer[6] == 0x1A && buffer[7] == 0x0A) {
            return Format::PNG;
        }

        if (isRAW(filename)) {
            return Format::RAW;
        }

        // Little Endian (49 49 2A 00)
        if (buffer[0] == 0x49 && buffer[1] == 0x49 && buffer[2] == 0x2A && buffer[3] == 0x00) {
            return Format::TIFF;
        }
        // Big Endian (4D 4D 00 2A)
        if (buffer[0] == 0x4D && buffer[1] == 0x4D && buffer[2] == 0x00 && buffer[3] == 0x2A) {
            return Format::TIFF;
        }

        return Format::UNDEFINED;
    }

    Bitmap open(const char *filename, SampleType sampleType, ColorModel colorModel, ColorProfile profile, Properties props) {
        Format format = getFormat(filename);

        Bitmap bitmap {};
 
        if (format == Format::JPG) {
            bitmap = loadJPEG(filename);
        }
        else if (format == Format::PNG) {
            bitmap = loadPNG(filename);
        }
        else if (format == Format::RAW) {
            bitmap = loadRAW(filename);
        }
        else if (format == Format::TIFF) {
            bitmap = loadTIFF(filename);
        } else {
            throw std::runtime_error("File format is not supported");
        }

        if (sampleType != bitmap.sampleType || colorModel != bitmap.colorModel || !profile.empty()) {
            return bitmap.convertTo(sampleType, colorModel, profile);
        }
        return bitmap;
    }

    void save(const char *filename, Bitmap &bitmap, Properties props) {
        std::string ext = getFileExtension(filename);
        
        if (ext == "jpg" || ext == "jpeg") {
            saveJPEG(filename, bitmap, props);
        }
        else if (ext == "png") {
            savePNG(filename, bitmap, props);
        }
        else if (ext == "tiff" || ext == "tif") {
            saveTIFF(filename, bitmap, props);
        }
        else {
            throw std::runtime_error("Unsupported file type. Supported types: jpg, jpeg, tiff, tif");
        }
    }
}

