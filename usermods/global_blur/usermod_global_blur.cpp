#include "wled.h"

class GlobalBlurUsermod : public Usermod {
  private:
    bool enabled = true;
    uint8_t blurAmount = 220;        // Интенсивность размытия / длина хвоста (0-254)
    uint8_t spatialPasses = 3;       // Количество проходов пространственного сглаживания

    CRGB* canvasBuffer = nullptr;    // Надежный буфер FastLED для работы с плавной графикой
    uint16_t numLeds = 0;

    void allocateBuffer() {
      uint16_t currentLeds = strip.getLengthTotal();
      if (currentLeds > 1500) currentLeds = 1500; // Защита памяти ESP32-C3
      if (currentLeds != numLeds) {
        if (canvasBuffer != nullptr) {
          delete[] canvasBuffer;
          canvasBuffer = nullptr;
        }
        if (currentLeds > 0) {
          canvasBuffer = new CRGB[currentLeds];
          numLeds = currentLeds;
          memset(canvasBuffer, 0, numLeds * sizeof(CRGB));
        } else {
          numLeds = 0;
        }
      }
    }

  public:
    void setup() override {
      allocateBuffer();
    }

    void loop() override {}

    // Используем post-processing перехватчик отрисовки
    void handleOverlayDraw() override {
      if (!enabled || canvasBuffer == nullptr) return;
      allocateBuffer();

      uint16_t totalLeds = strip.getLengthTotal();
      if (totalLeds != numLeds) return;

      // 1. Считываем текущий кадр из ленты в наш рабочий буфер
      for (uint16_t i = 0; i < numLeds; i++) {
        uint32_t c = strip.getPixelColor(i);
        CRGB current(r(c), g(c), b(c));

        // Накладываем временной шлейф (Motion Blur) с помощью честного смешивания FastLED
        if (blurAmount > 0) {
          canvasBuffer[i].nscale8(blurAmount); // Ослабляем старый кадр
          canvasBuffer[i] += current;          // Добавляем новый цвет
        } else {
          canvasBuffer[i] = current;
        }
      }

      // 2. Применяем многопроходное пространственное размытие по всему буферу
      for (uint8_t p = 0; p < spatialPasses; p++) {
        bluruyan(canvasBuffer, numLeds, 64); // Плавное размытие соседей
      }

      // 3. Выгружаем сглаженный и размытый кадр обратно на ленту
      for (uint16_t i = 0; i < numLeds; i++) {
        strip.setPixelColor(i, canvasBuffer[i].r, canvasBuffer[i].g, canvasBuffer[i].b);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) top = root.createNestedObject("Global_Blur");
      top["enabled"] = enabled;
      top["blur_amount"] = blurAmount;
      top["spatial_passes"] = spatialPasses;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) return false;
      getJsonValue(top["enabled"], enabled);
      getJsonValue(top["blur_amount"], blurAmount);
      getJsonValue(top["spatial_passes"], spatialPasses);
      return true;
    }
    
    uint16_t getId() override {
      return 6969; 
    }
};

static GlobalBlurUsermod myBlurUsermod;
REGISTER_USERMOD(myBlurUsermod);
