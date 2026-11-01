#pragma once
#include "wled.h"

class GlobalBlurUsermod : public Usermod {
  private:
    bool enabled = false;
    uint8_t spatialBlurAmount = 64;  // Сглаживание между пикселями (0-255)
    uint8_t motionBlurAmount = 150;  // Сила шлейфа / моушен блюра (0-250)

    uint32_t* prevFrame = nullptr;   // Наш буфер для хранения предыдущего кадра
    uint16_t numLeds = 0;

    // Быстрая функция смешивания текущего и предыдущего цветов
    uint32_t blendColors(uint32_t oldC, uint32_t newC, uint8_t motionBlur) {
      if (motionBlur == 0) return newC;
      // Ограничиваем макс значение до 250, иначе на 255 анимация "заморозится" навсегда
      if (motionBlur > 250) motionBlur = 250; 

      // Распаковываем цвета (поддерживает ленты с белым каналом - RGBW)
      uint8_t wO = (oldC >> 24) & 0xFF, rO = (oldC >> 16) & 0xFF, gO = (oldC >> 8) & 0xFF, bO = oldC & 0xFF;
      uint8_t wN = (newC >> 24) & 0xFF, rN = (newC >> 16) & 0xFF, gN = (newC >> 8) & 0xFF, bN = newC & 0xFF;

      uint8_t keep = motionBlur;          // Сколько оставляем от старого кадра
      uint8_t add  = 255 - motionBlur;    // Сколько берем от нового

      // Сдвиг >> 8 работает намного быстрее обычного деления на 256 (для ESP32-C3 топ)
      uint8_t w = ((wO * keep) + (wN * add)) >> 8;
      uint8_t r = ((rO * keep) + (rN * add)) >> 8;
      uint8_t g = ((gO * keep) + (gN * add)) >> 8;
      uint8_t b = ((bO * keep) + (bN * add)) >> 8;

      return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    // Следим за выделением памяти, если вдруг изменишь длину ленты в настройках
    void allocateBuffer() {
      uint16_t currentLeds = strip.getLengthTotal();
      if (currentLeds != numLeds) {
        if (prevFrame != nullptr) {
          free(prevFrame);
          prevFrame = nullptr;
        }
        if (currentLeds > 0) {
          prevFrame = (uint32_t*)malloc(currentLeds * sizeof(uint32_t));
          if (prevFrame != nullptr) {
            numLeds = currentLeds;
            memset(prevFrame, 0, numLeds * sizeof(uint32_t)); // Заполняем черным
          } else {
            numLeds = 0; // Защита на случай, если память забита
          }
        } else {
          numLeds = 0;
        }
      }
    }

  public:
    void setup() override {
      allocateBuffer();
    }

    void loop() override {
      // Раз в секунду проверяем, не добавил ли ты диодов
      EVERY_N_SECONDS(1) {
        allocateBuffer();
      }
    }

    void handleOverlayDraw() override {
      if (!enabled) return;
      
      // 1. Пространственное размытие (Spatial Blur) - сглаживаем границы внутри текущего кадра
      if (spatialBlurAmount > 0) {
        for (uint8_t i = 0; i < strip.getMaxSegments(); i++) {
          Segment& seg = strip.getSegment(i);
          if (seg.isActive()) {
            seg.blur(spatialBlurAmount);
          }
        }
      }

      // 2. Временное размытие (Motion Blur) - накладываем шлейф
      if (motionBlurAmount > 0 && prevFrame != nullptr) {
        for (uint16_t i = 0; i < numLeds; i++) {
          uint32_t newColor = strip.getPixelColor(i);
          uint32_t blendedColor = blendColors(prevFrame[i], newColor, motionBlurAmount);
          
          strip.setPixelColor(i, blendedColor); // Отправляем на ленту
          prevFrame[i] = blendedColor;          // Запоминаем для следующего тика
        }
      } else if (prevFrame != nullptr) {
        // Если моушен блюр выключили ползунком, просто держим буфер актуальным
        for (uint16_t i = 0; i < numLeds; i++) {
          prevFrame[i] = strip.getPixelColor(i);
        }
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject("Global_Blur");
      top["enabled"] = enabled;
      top["spatial_amount"] = spatialBlurAmount;
      top["motion_amount"] = motionBlurAmount;
    }

    void readFromConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) return;
      getJsonValue(top["enabled"], enabled);
      getJsonValue(top["spatial_amount"], spatialBlurAmount);
      getJsonValue(top["motion_amount"], motionBlurAmount);
    }
    
    // В V2 архитектуре каждому юзермоду нужен ID
    uint16_t getId() override {
      return 6969; // Любое уникальное число
    }
};
