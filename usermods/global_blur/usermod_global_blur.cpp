#pragma once
#include "wled.h"

class GlobalBlurUsermod : public Usermod {
private:
  bool enabled = true;

  // Временное сглаживание (0–255). 0 — без накопления, 255 — очень длинный хвост.
  uint8_t blurAmount = 200;

  // Количество проходов пространственного размытия (0–4).
  // Каждый проход расширяет радиус на 1 пиксель в каждую сторону.
  uint8_t spatialPasses = 2;

  CRGB* canvasBuffer = nullptr; // буфер для накопления и первого прохода
  CRGB* tempBuffer   = nullptr; // временный буфер для второго и последующих проходов
  uint16_t numLeds = 0;

  void allocateBuffers() {
    uint16_t currentLeds = strip.getLengthTotal();
    if (currentLeds > 1500) currentLeds = 1500; // ограничение для ESP32-C3

    if (currentLeds == numLeds) return;

    // Освобождаем старые буферы
    delete[] canvasBuffer;
    delete[] tempBuffer;
    canvasBuffer = nullptr;
    tempBuffer   = nullptr;

    if (currentLeds > 0) {
      canvasBuffer = new CRGB[currentLeds];
      tempBuffer   = new CRGB[currentLeds];
      numLeds = currentLeds;
      memset(canvasBuffer, 0, numLeds * sizeof(CRGB));
      memset(tempBuffer,   0, numLeds * sizeof(CRGB));
    } else {
      numLeds = 0;
    }
  }

public:
  void setup() override {
    allocateBuffers();
  }

  void loop() override {}

  void handleOverlayDraw() override {
    if (!enabled || numLeds == 0) return;

    uint16_t totalLeds = strip.getLengthTotal();
    if (totalLeds != numLeds) {
      allocateBuffers();
      if (numLeds == 0) return;
    }

    // 1. Считываем кадр и применяем временное сглаживание (running average).
    for (uint16_t i = 0; i < numLeds; i++) {
      uint32_t current = strip.getPixelColor(i);
      if (blurAmount > 0) {
        CRGB& buf = canvasBuffer[i];
        // Масштабируем буфер и добавляем новый цвет с насыщением.
        // Умножение на blurAmount и сдвиг >>8 работают быстрее, чем nscale8.
        uint8_t r = (buf.r * blurAmount) >> 8;
        uint8_t g = (buf.g * blurAmount) >> 8;
        uint8_t b = (buf.b * blurAmount) >> 8;
        buf.r = qadd8(r, R(current));
        buf.g = qadd8(g, G(current));
        buf.b = qadd8(b, B(current));
      } else {
        canvasBuffer[i] = CRGB(R(current), G(current), B(current));
      }
    }

    // 2. Пространственное размытие: несколько проходов ядра (1, 2, 1) / 4.
    // Это эквивалентно сглаживанию с сохранением энергии, без делений (только >>2).
    CRGB* src = canvasBuffer;
    CRGB* dst = tempBuffer;

    for (uint8_t pass = 0; pass < spatialPasses; pass++) {
      for (uint16_t i = 0; i < numLeds; i++) {
        const CRGB& prev = (i == 0) ? src[0] : src[i - 1];
        const CRGB& cur  = src[i];
        const CRGB& next = (i == numLeds - 1) ? src[numLeds - 1] : src[i + 1];

        dst[i].r = (prev.r + 2 * cur.r + next.r) >> 2;
        dst[i].g = (prev.g + 2 * cur.g + next.g) >> 2;
        dst[i].b = (prev.b + 2 * cur.b + next.b) >> 2;
      }
      // Меняем буферы местами, чтобы следующий проход читал результат текущего
      CRGB* swap = src;
      src = dst;
      dst = swap;
    }

    // После всех проходов результат лежит в src (если spatialPasses > 0)
    // или в canvasBuffer (если spatialPasses == 0).
    CRGB* result = (spatialPasses > 0) ? src : canvasBuffer;

    // 3. Записываем итоговый кадр обратно в ленту.
    for (uint16_t i = 0; i < numLeds; i++) {
      strip.setPixelColor(i, result[i].r, result[i].g, result[i].b);
    }
  }

  // --- Конфигурация (веб-интерфейс) ---
  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("Global_Blur");
    top["enabled"]         = enabled;
    top["blur_amount"]     = blurAmount;
    top["spatial_passes"]  = spatialPasses;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root["Global_Blur"];
    if (top.isNull()) return false;

    getJsonValue(top["enabled"],        enabled);
    getJsonValue(top["blur_amount"],    blurAmount);
    getJsonValue(top["spatial_passes"], spatialPasses);

    // Ограничиваем значения разумными диапазонами
    if (blurAmount > 254) blurAmount = 254;
    if (spatialPasses > 4) spatialPasses = 4;

    return true;
  }

  uint16_t getId() override {
    return 6969; // уникальный ID (не конфликтует с системными)
  }
};

static GlobalBlurUsermod myBlurUsermod;
REGISTER_USERMOD(myBlurUsermod);
