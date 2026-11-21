#include <GU256X64D7000.h>
#include <SD.h>

#include "WindowsBmp.h"

// 5 V Arduino pins: D2–D9 for VFD data, A0 for PBUSY,
// A1 for /WR, and A2 for /RESET. Reserve D10 for SD chip select
// and use the board's hardware SPI pins for the SD module.
using Wiring = gu256x64d7000::ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, A0, A1, A2>;
gu256x64d7000::Display<Wiring> vfd;
constexpr uint8_t sdChipSelect = 10;

void setup() {
  Serial.begin(115200);
  vfd.begin();
  if (!vfd.initialize()) {
    Serial.println(F("VFD did not initialize"));
    return;
  }
  if (!SD.begin(sdChipSelect)) {
    Serial.println(F("SD card did not initialize"));
    return;
  }
  File image = SD.open("IMAGE.BMP", FILE_READ);
  if (!image) {
    Serial.println(F("Cannot open IMAGE.BMP"));
    return;
  }

  const bool drawn = sd_bitmap::drawWindowsBmp(
      image, [](uint16_t columns, uint8_t band, const uint8_t* bytes) {
        return vfd.setCursor(0, band) && vfd.beginBitmap(columns, 1) &&
               vfd.writeBytes(bytes, columns);
      });
  image.close();
  Serial.println(drawn ? F("Image drawn")
                       : F("Unsupported BMP, read failure, or VFD timeout"));
}

void loop() {}
