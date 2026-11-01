#include "wled.h"

class GlobalBlurUsermod : public Usermod {
  private:
    bool enabled = true;
    uint8_t spatialBlurAmount = 172; // Сила смешивания (0-255)
    uint8_t spatialPasses = 4;       // Радиус/ширина размытия (1-20 проходов)
    uint8_t motionBlurAmount = 210;  // Длина шлейфа во времени (0-254)

    uint32_t* prevFrame = nullptr;   
    uint16_t numLeds = 0;

    static inline uint32_t blendColors(uint32_t oldC, uint32_t newC, uint8_t keep) {
      if (keep == 0) return newC;
      if (keep > 254) keep = 254; 
      uint16_t add = 256 - keep;

      uint8_t w = (((oldC >> 24) & 0xFF) * keep + ((newC >> 24) & 0xFF) * add) >> 8;
      uint8_t r = (((oldC >> 16) & 0xFF) * keep + ((newC >> 16) & 0xFF) * add) >> 8;
      uint8_t g = (((oldC >> 8)  & 0xFF) * keep + ((newC >> 8)  & 0xFF) * add) >> 8;
      uint8_t b = (( oldC        & 0xFF) * keep + ( newC        & 0xFF) * add) >> 8;

      return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    void allocateBuffer() {
      uint16_t currentLeds = strip.getLengthTotal();
      if (currentLeds > 1200) currentLeds = 1200;
      if (currentLeds != numLeds) {
        if (prevFrame != nullptr) {
          free(prevFrame);
          prevFrame = nullptr;
        }
        if (currentLeds > 0) {
          prevFrame = (uint32_t*)malloc(currentLeds * sizeof(uint32_t));
          if (prevFrame != nullptr) {
            numLeds = currentLeds;
            memset(prevFrame, 0, numLeds * sizeof(uint32_t));
          } else {
            numLeds = 0;
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

    void loop() override {}

    void handleOverlayDraw() override {
      if (!enabled) return;
      allocateBuffer();

      uint8_t passes = (spatialPasses > 20) ? 20 : spatialPasses;
      uint8_t maxSeg = strip.getMaxSegments();

      for (uint8_t s = 0; s < maxSeg; s++) {
        Segment& seg = strip.getSegment(s);
        if (!seg.isActive()) continue;

        // 1. Временное сглаживание (Motion Blur) ДО пространственного,
        // чтобы шлейфы тоже мягко размывались в стороны
        if (prevFrame != nullptr) {
          uint16_t segLen = seg.length();
          for (uint16_t i = 0; i < segLen; i++) {
            uint16_t absIndex = seg.start + i;
            if (absIndex >= numLeds) break;

            uint32_t newColor = seg.getPixelColor(i);
            if (motionBlurAmount > 0) {
              uint32_t blended = blendColors(prevFrame[absIndex], newColor, motionBlurAmount);
              seg.setPixelColor(i, blended);
              prevFrame[absIndex] = blended;
            } else {
              prevFrame[absIndex] = newColor;
            }
          }
        }

        // 2. Многопроходное пространственное размытие (широкий Gaussian Blur)
        if (spatialBlurAmount > 0 && passes > 0) {
          for (uint8_t p = 0; p < passes; p++) {
            seg.blur(spatialBlurAmount);
          }
        }
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) top = root.createNestedObject("Global_Blur");
      top["enabled"] = enabled;
      top["spatial_amount"] = spatialBlurAmount;
      top["spatial_passes"] = spatialPasses;
      top["motion_amount"] = motionBlurAmount;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) return false;
      getJsonValue(top["enabled"], enabled);
      getJsonValue(top["spatial_amount"], spatialBlurAmount);
      getJsonValue(top["spatial_passes"], spatialPasses);
      getJsonValue(top["motion_amount"], motionBlurAmount);
      return true;
    }
    
    uint16_t getId() override {
      return 6969; 
    }
};

static GlobalBlurUsermod myBlurUsermod;
REGISTER_USERMOD(myBlurUsermod);
