#pragma once

#include <stdint.h>

namespace sd_bitmap {

constexpr uint16_t panelWidth = 256;
constexpr uint8_t panelHeight = 64;

inline uint16_t readLE16(const uint8_t* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

inline uint32_t readLE32(const uint8_t* p) {
  return uint32_t(readLE16(p)) | (uint32_t(readLE16(p + 2)) << 16);
}

// Brightness of one BGR pixel, multiplied by 256.
inline uint16_t brightness(const uint8_t* bgr) {
  return 29u * bgr[0] + 150u * bgr[1] + 77u * bgr[2];
}

// Input supports the Arduino File interface. DrawBand receives (width,
// band index, column bytes) and returns false if the display times out.
template <typename Input, typename DrawBand>
bool drawWindowsBmp(Input& input, DrawBand drawBand) {
  constexpr uint32_t fileHeaderSize = 14;
  uint8_t header[fileHeaderSize + 40];
  if (!input.seek(0) ||
      input.read(header, sizeof(header)) != int(sizeof(header)) ||
      header[0] != 'B' || header[1] != 'M') {
    return false;
  }

  const uint32_t fileSize = input.size();
  const uint32_t pixels = readLE32(header + 10);
  const uint32_t dibSize = readLE32(header + 14);
  const uint32_t width = readLE32(header + 18);
  const uint32_t signedHeight = readLE32(header + 22);
  const bool topDown = (signedHeight & 0x80000000ul) != 0;
  const uint32_t height = topDown ? 0u - signedHeight : signedHeight;
  const uint16_t depth = readLE16(header + 28);
  if (width == 0 || width > panelWidth || height == 0 || height > panelHeight ||
      readLE16(header + 26) != 1 || (depth != 1 && depth != 24) ||
      readLE32(header + 30) != 0 || pixels < sizeof(header) ||
      pixels > fileSize || dibSize < 40 || dibSize > pixels - fileHeaderSize) {
    return false;
  }

  // BMP rows occupy whole 32-bit words, even when the last word is partial.
  const uint16_t rowStride = ((width * depth + 31) / 32) * 4;
  if (height * rowStride > fileSize - pixels) {
    return false;
  }

  bool lightIndex[2] = {};
  if (depth == 1) {
    const uint32_t paletteOffset = fileHeaderSize + dibSize;
    const uint32_t paletteCount = readLE32(header + 46);
    uint8_t palette[8];
    if ((paletteCount != 0 && paletteCount != 2) ||
        pixels - paletteOffset < sizeof(palette) ||
        !input.seek(paletteOffset) ||
        input.read(palette, sizeof(palette)) != int(sizeof(palette))) {
      return false;
    }
    const uint16_t first = brightness(palette);
    const uint16_t second = brightness(palette + 4);
    lightIndex[0] = first > second;
    lightIndex[1] = second > first;
  }

  const uint8_t packedWidth = (width + 7) / 8;
  for (uint8_t firstRow = 0; firstRow < height; firstRow += 8) {
    uint8_t band[panelWidth] = {};
    for (uint8_t row = 0; row < 8 && firstRow + row < height; ++row) {
      const uint8_t y = firstRow + row;
      const uint8_t fileRow = topDown ? y : height - 1 - y;
      if (!input.seek(pixels + uint32_t(fileRow) * rowStride)) {
        return false;
      }

      const uint8_t mask = 0x80u >> row;
      if (depth == 1) {
        uint8_t packed[panelWidth / 8];
        if (input.read(packed, packedWidth) != packedWidth) {
          return false;
        }
        for (uint16_t x = 0; x < width; ++x) {
          const bool index = (packed[x / 8] & (0x80u >> (x % 8))) != 0;
          if (lightIndex[index]) {
            band[x] |= mask;
          }
        }
      } else {
        for (uint16_t x = 0; x < width; ++x) {
          uint8_t bgr[3];
          if (input.read(bgr, sizeof(bgr)) != int(sizeof(bgr))) {
            return false;
          }
          if (brightness(bgr) >= 128u * 256u) {
            band[x] |= mask;
          }
        }
      }
    }

    if (!drawBand(width, firstRow / 8, band)) {
      return false;
    }
  }
  return true;
}

}  // namespace sd_bitmap
