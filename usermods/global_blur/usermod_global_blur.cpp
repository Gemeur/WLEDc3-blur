#pragma once
#include "wled.h"

class GlobalBlurUsermod : public Usermod {
private:
  bool enabled = true;
  uint8_t blurAmount    = 200;  // 0 = без шлейфа, 255 = очень длинный хвост
  uint8_t spatialPasses = 2;    // 0..4

  CRGB* accumBuffer = nullptr;  // ТОЛЬКО temporal-накопление, сюда никогда не пишем размытие
  CRGB* workBuffer  = nullptr;  // рабочая копия для пространственного размытия
  CRGB* tempBuffer  = nullptr;  // ping-pong буфер
  uint16_t numLeds = 0;

  void allocateBuffers() {
    uint16_t n = strip.getLengthTotal();
    if (n > 1500) n = 1500;
    if (n == numLeds) return;

    delete[] accumBuffer; accumBuffer = nullptr;
    delete[] workBuffer;  workBuffer  = nullptr;
    delete[] tempBuffer;  tempBuffer  = nullptr;

    if (n > 0) {
      accumBuffer = new CRGB[n];
      workBuffer  = new CRGB[n];
      tempBuffer  = new CRGB[n];
      numLeds = n;
      memset(accumBuffer, 0, n * sizeof(CRGB));
    } else {
      numLeds = 0;
    }
  }

public:
  void setup() override { allocateBuffers(); }
  void loop() override {}

  void handleOverlayDraw() override {
    if (!enabled || numLeds == 0) return;
    if (strip.getLengthTotal() != numLeds) {
      allocateBuffers();
      if (numLeds == 0) return;
    }

    // === 1. Temporal: взвешенное среднее БЕЗ насыщения и БЕЗ пересвета ===
    // accum = (accum * k + current * (256 - k)) >> 8
    // В установившемся режиме accum == current, ни пересвета, ни потускнения.
    const uint16_t k    = blurAmount;
    const uint16_t invK = 256 - blurAmount;
    for (uint16_t i = 0; i < numLeds; i++) {
      uint32_t cur = strip.getPixelColor(i);
      CRGB& a = accumBuffer[i];
      a.r = (a.r * k + R(cur) * invK) >> 8;
      a.g = (a.g * k + G(cur) * invK) >> 8;
      a.b = (a.b * k + B(cur) * invK) >> 8;
    }

    // === 2. Spatial: размытие РАБОЧЕЙ КОПИИ (без обратной связи!) ===
    CRGB* src;
    CRGB* dst;
    if (spatialPasses == 0) {
      src = accumBuffer;
    } else {
      memcpy(workBuffer, accumBuffer, numLeds * sizeof(CRGB));
      src = workBuffer;
      dst = tempBuffer;

      for (uint8_t pass = 0; pass < spatialPasses; pass++) {
        for (uint16_t i = 0; i < numLeds; i++) {
          const CRGB& prev = (i == 0)           ? src[0]           : src[i - 1];
          const CRGB& cur  = src[i];
          const CRGB& next = (i == numLeds - 1) ? src[numLeds - 1] : src[i + 1];

          // Округление к ближайшему: +2 перед >>2 сохраняет энергию
          dst[i].r = (prev.r + 2 * cur.r + next.r + 2) >> 2;
          dst[i].g = (prev.g + 2 * cur.g + next.g + 2) >> 2;
          dst[i].b = (prev.b + 2 * cur.b + next.b + 2) >> 2;
        }
        CRGB* sw = src; src = dst; dst = sw;
      }
    }

    // === 3. Вывод ===
    for (uint16_t i = 0; i < numLeds; i++) {
      strip.setPixelColor(i, src[i].r, src[i].g, src[i].b);
    }
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("Global_Blur");
    top["enabled"]        = enabled;
    top["blur_amount"]    = blurAmount;
    top["spatial_passes"] = spatialPasses;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root["Global_Blur"];
    if (top.isNull()) return false;
    getJsonValue(top["enabled"],        enabled);
    getJsonValue(top["blur_amount"],    blurAmount);
    getJsonValue(top["spatial_passes"], spatialPasses);
    if (blurAmount    > 255) blurAmount    = 255;
    if (spatialPasses > 4)   spatialPasses = 4;
    return true;
  }

  uint16_t getId() override { return 6969; }
};

static GlobalBlurUsermod myBlurUsermod;
REGISTER_USERMOD(myBlurUsermod);
