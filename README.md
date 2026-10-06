# ImageIO

ImageIO is a C++ image I/O and color-management library built around a unified
`Bitmap` representation. It provides a common API for reading and writing
JPEG, PNG, TIFF, and RAW images while handling pixel formats, color models,
and ICC color profiles.

The library is built on top of libjpeg-turbo, libpng, libtiff, LibRaw, and
LittleCMS and is designed to provide direct control over image data while
remaining easy to integrate into CMake-based projects.
 
![Build Status](https://github.com/dmytro-serhiichuk/ImageIO/actions/workflows/ci.yml/badge.svg)
![License](https://img.shields.io/github/license/dmytro-serhiichuk/ImageIO)
 
> **Status:** personal project, built and tested on Windows only. 
See [Limitations](#limitations) before relying on it for anything beyond experimentation.

## Features
 
- Unified read/write API across JPEG, PNG, TIFF, and RAW (via libjpeg-turbo, 
libpng, libtiff, LibRaw)
- Bit-depth conversion (8/16/32-bit integer, float) and color-model conversion 
(RGB, Gray, CMYK, XYZ, and their alpha variants)
- ICC profile-aware color management via LittleCMS, with built-in profiles 
(sRGB, Adobe RGB, ProPhoto RGB, CMYK, XYZ, Gray) and support for 
custom/embedded profiles
- Direct interop with raw `lcms2` handles for advanced color-management use cases
- Static and shared library targets, consumable via CMake `find_package`
- Automated build and test pipeline with GitHub Actions

## Supported Formats
 
| Format | Read | Write |
|---|---|---|
| **JPEG** | 8-bit only; Gray, RGB, YCbCr, CMYK, YCCK | 8-bit only; RGB, Gray, CMYK (4:4:4 chroma subsampling, grayscale sampling for Gray) |
| **PNG** | 1/2/4-bit (expanded to 8-bit), 8-bit, 16-bit; Palette, Gray, Gray+Alpha, RGB, RGB+Alpha | 8-bit, 16-bit; RGB, RGBA, Gray, Gray+Alpha (other models auto-converted) |
| **TIFF** | 1–32-bit; uint/int/float samples; MinIsBlack, MinIsWhite, RGB, Separated (CMYK), YCbCr | All `ColorModel` values except XYZ (converted to RGB); all `SampleType` depths |
| **RAW** | Always decoded to 16-bit, then converted as requested | Not supported (read-only format) |

**Format notes:**
- **Write conversions are non-destructive.** If a `Bitmap`'s color model isn't 
natively supported by the target format (e.g., saving a CMYK `Bitmap` as PNG, 
or a Gray+Alpha `Bitmap` as JPEG), `save()` converts internally for the write 
only - the `Bitmap` instance passed in is left unchanged.
- **JPEG** - on write, Gray+Alpha is converted to Gray and CMYK+Alpha is 
converted to CMYK (JPEG has no alpha channel).
- **TIFF** - palette images are **not supported for reading**. Tiled and 
stripped layouts are both supported, as is `PLANARCONFIG_SEPARATE`. Associated 
and unassociated alpha (`EXTRASAMPLE_ASSOCALPHA` / `EXTRASAMPLE_UNASSALPHA`) are 
both handled correctly and normalized to a single representation. Integer 
samples are normalized to unsigned; bit depths other than 8/16/32 are normalized 
up to the nearest of those.
- **RAW** - decoded using a fixed XYZ-based profile with a D65 white point. 
Crop and orientation metadata are applied automatically.

## Design Notes
 
`Bitmap` intentionally exposes its internals (`buffer`, `bufferSize`, 
dimensions, `sampleType`, `colorModel`, `profile`) as public, mutable fields 
rather than hiding them behind a fully encapsulated interface. This is a 
deliberate trade-off: it allows direct pixel manipulation, custom profile 
assignment, and buffer replacement without going through a constrained API - 
at the cost of safety. The library does not validate that these fields stay 
consistent with each other (e.g., that `bufferSize` matches `width * height * 
samplesPerPixel * bytesPerSample`). Violating this invariant produces incorrect 
output or an error from downstream functions, not a clear validation message - 
keeping fields consistent is the caller's responsibility.
 
`Properties` currently only partially affects output: `jpegQuality` is 
functional; `blendAlpha` and `backgroundColor` are reserved for future 
alpha-blending support and have no effect yet.
 
**Error handling.** Incompatible combinations of `ColorModel` and `ColorProfile` 
(e.g., an RGB color model paired with a CMYK-based profile) are rejected with 
`std::runtime_error("Bitmap: Invalid output icc profile")` rather than silently 
producing incorrect output.

## Architecture

ImageIO separates format-specific I/O from the common in-memory image
representation.

- `ImageIO::open()` detects the input format and decodes it into `Bitmap`.
- `Bitmap` stores pixel data, sample type, color model, and color profile.
- Format-specific backends handle encoding and decoding.
- LittleCMS is used for ICC profile transformations.
- `ImageIO::save()` converts the bitmap to a format-compatible representation
  when necessary before encoding.

## Requirements
 
- C++11 or later
- CMake 3.x
- vcpkg (recommended)
- Dependencies: 
[libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo), 
[libpng](http://www.libpng.org/pub/png/libpng.html), 
[libtiff](http://www.libtiff.org/), 
[LibRaw](https://www.libraw.org/), 
[Little CMS (lcms2)](https://www.littlecms.com/)

## Installation
 
The recommended way to get the dependencies is [vcpkg](https://vcpkg.io), in 
manifest mode - this is how the project is built in CI:
 
```sh
git clone https://github.com/dmytro-serhiichuk/ImageIO.git
cd ImageIO
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```
 
Alternatively, the five dependencies can be provided through any method CMake 
can discover via `find_package` (e.g., libraries built and installed to a 
common prefix added to `CMAKE_PREFIX_PATH`).
 
### Running tests
 
```sh
ctest --test-dir build -C Release --output-on-failure
```
 
## Integration
 
```cmake
find_package(ImageIO REQUIRED)
 
target_link_libraries(your_target PRIVATE ImageIO::ImageIO_static)
# or, for the shared build:
target_link_libraries(your_target PRIVATE ImageIO::ImageIO_shared)
# ImageIO::ImageIO is an alias for the shared target
```

## Usage Example
 
```cpp
#include <ImageIO/image-io.h>
#include <ImageIO/color-profile.h>
 
const char* filename = "image.png";
 
// Define a color space for the image
auto adobeRGB = ImageIO::ColorProfile::AdobeRGB();
 
ImageIO::Bitmap bitmap = ImageIO::open(
    filename,
    ImageIO::SampleType::U16,
    ImageIO::ColorModel::RGB,
    adobeRGB
);
 
auto bitmapCMYK = bitmap.convertColorModel(ImageIO::ColorModel::CMYK);
 
ImageIO::Properties props{};
props.jpegQuality = 100;
 
ImageIO::save("result1.tiff", bitmap);
ImageIO::save("result2.jpg", bitmapCMYK, props);
```

## API Overview
 
| Header | Contents |
|---|---|
| `image-io.h` | Core entry points: `open()`, `save()`, `getFormat()` |
| `bitmap.h` | `Bitmap` - the central image container and in-memory conversion API |
| `color-model.h` | `ColorModel` enum and channel/alpha helper functions |
| `sample-type.h` | `SampleType` enum (bit depth) and byte-size helper |
| `color-profile.h` | `ColorProfile` - a wrapper around ICC profiles (LittleCMS-backed), with factory methods for common color spaces |
| `color-profile-lcms.h` | Interop helper for working with raw `lcms2` (`cmsHPROFILE`) handles directly |

## Limitations
 
- Built and tested on Windows only (CI currently targets `windows-latest`)
- RAW decoding uses a fixed XYZ-based profile with a D65 white point
- `Properties::blendAlpha` / `backgroundColor` are not yet implemented
- TIFF palette images are not supported for reading
- `Bitmap`'s fields are publicly mutable and not validated for consistency - 
see [Design Notes](#design-notes)
## License
 
This project is licensed under the terms described in [LICENSE](./LICENSE).
