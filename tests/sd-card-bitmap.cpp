#if defined(GU256X64D7000_HOST_TEST)

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "WindowsBmp.h"

namespace {
using Bytes = std::vector<uint8_t>;

struct MemoryFile {
  explicit MemoryFile(const Bytes& data) : data(data) {}

  bool seek(uint32_t offset) {
    position = offset;
    return offset <= data.size();
  }

  int read(uint8_t* out, unsigned count) {
    if (position > data.size() || count > data.size() - position) {
      return -1;
    }
    memcpy(out, data.data() + position, count);
    position += count;
    return count;
  }

  uint32_t size() const { return data.size(); }

  const Bytes& data;
  uint32_t position = 0;
};

void putLE(Bytes& bmp, unsigned offset, uint32_t value, unsigned size = 4) {
  for (unsigned i = 0; i < size; ++i) {
    bmp[offset + i] = value & 0xff;
    value >>= 8;
  }
}

Bytes header(unsigned width, int height, unsigned depth, unsigned pixelOffset,
             unsigned size) {
  Bytes bmp(size);
  bmp[0] = 'B';
  bmp[1] = 'M';
  putLE(bmp, 2, size);
  putLE(bmp, 10, pixelOffset);
  putLE(bmp, 14, 40);
  putLE(bmp, 18, width);
  putLE(bmp, 22, height);
  putLE(bmp, 26, 1, 2);
  putLE(bmp, 28, depth, 2);
  return bmp;
}

using Bands = std::vector<Bytes>;

bool draw(const Bytes& bmp, Bands& bands) {
  MemoryFile file(bmp);
  return sd_bitmap::drawWindowsBmp(
      file, [&](uint16_t width, uint8_t band, const uint8_t* bytes) {
        assert(band == bands.size());
        bands.emplace_back(bytes, bytes + width);
        return true;
      });
}

void monochrome(bool firstIsLight) {
  // Nine pixels occupy two bytes followed by two bytes of padding.
  // A positive height stores row 8 first and row 0 last.
  constexpr unsigned offset = 62;
  Bytes bmp = header(9, 9, 1, offset, offset + 36);
  const unsigned lightEntry = firstIsLight ? 54 : 58;
  bmp[lightEntry] = bmp[lightEntry + 1] = bmp[lightEntry + 2] = 0xff;

  const uint8_t rows[][2] = {
      {0x00, 0x80},  // row 8
      {0x80, 0x00},  // row 7
      {0, 0},       {0, 0}, {0, 0},       {0, 0},
      {0, 0},       {0, 0}, {0x80, 0x80},  // row 0
  };
  for (unsigned row = 0; row < 9; ++row) {
    for (unsigned byte = 0; byte < 2; ++byte) {
      bmp[offset + 4 * row + byte] =
          firstIsLight ? ~rows[row][byte] : rows[row][byte];
    }
    bmp[offset + 4 * row + 2] = 0xaa;
    bmp[offset + 4 * row + 3] = 0x55;
  }

  Bands bands;
  assert(draw(bmp, bands));
  const Bands expected{{0x81, 0, 0, 0, 0, 0, 0, 0, 0x80},
                       {0, 0, 0, 0, 0, 0, 0, 0, 0x80}};
  assert(bands == expected);
}

void topDownColor() {
  constexpr unsigned offset = 54;
  Bytes bmp = header(3, -2, 24, offset, offset + 24);
  const uint8_t pixels[] = {
      0,   110, 255, 0, 255, 0, 127, 127, 127, 0xff, 0xff, 0xff,
      128, 128, 128, 0, 0,   0, 255, 255, 255, 0xff, 0xff, 0xff,
  };
  memcpy(bmp.data() + offset, pixels, sizeof(pixels));
  Bands bands;
  assert(draw(bmp, bands));
  const Bands expected{{0xc0, 0x80, 0x40}};
  assert(bands == expected);

  MemoryFile file(bmp);
  assert(!sd_bitmap::drawWindowsBmp(
      file, [](uint16_t, uint8_t, const uint8_t*) { return false; }));

  bands.clear();
  bmp.pop_back();
  assert(!draw(bmp, bands) && bands.empty());  // truncated pixel data
}

void unsupported() {
  Bands bands;
  Bytes bmp = header(9, 1, 1, 54, 58);
  assert(!draw(bmp, bands) && bands.empty());  // missing palette
  bmp = header(257, 1, 24, 54, 826);
  assert(!draw(bmp, bands) && bands.empty());  // panel width exceeded
  bmp = header(1, 1, 24, 54, 58);
  bmp[30] = 1;
  assert(!draw(bmp, bands) && bands.empty());  // compression
}
}  // namespace

int main() {
  monochrome(true);
  monochrome(false);
  topDownColor();
  unsupported();
}

#endif
