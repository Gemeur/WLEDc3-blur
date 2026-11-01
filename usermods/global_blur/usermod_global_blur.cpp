#include "wled.h"

class GlobalBlurUsermod : public Usermod {
  private:
    bool enabled = false;
    uint8_t spatialBlurAmount = 64; 
    uint8_t motionBlurAmount = 150; 

    uint32_t* prevFrame = nullptr;   
    uint16_t numLeds = 0;

    uint32_t blendColors(uint32_t oldC, uint32_t newC, uint8_t motionBlur) {
      if (motionBlur == 0) return newC;
      if (motionBlur > 250) motionBlur = 250; 

      uint8_t wO = (oldC >> 24) & 0xFF, rO = (oldC >> 16) & 0xFF, gO = (oldC >> 8) & 0xFF, bO = oldC & 0xFF;
      uint8_t wN = (newC >> 24) & 0xFF, rN = (newC >> 16) & 0xFF, gN = (newC >> 8) & 0xFF, bN = newC & 0xFF;

      uint8_t keep = motionBlur;          
      uint8_t add  = 255 - motionBlur;    

      uint8_t w = ((wO * keep) + (wN * add)) >> 8;
      uint8_t r = ((rO * keep) + (rN * add)) >> 8;
      uint8_t g = ((gO * keep) + (gN * add)) >> 8;
      uint8_t b = ((bO * keep) + (bN * add)) >> 8;

      return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

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

    void loop() override {
      // Оставляем пустым, проверка перенесена в отрисовку
    }

    void handleOverlayDraw() override {
      if (!enabled) return;
      
      allocateBuffer(); // Если изменили число диодов в настройках — перевыделяем буфер

      if (spatialBlurAmount > 0) {
        for (uint8_t i = 0; i < strip.getMaxSegments(); i++) {
          Segment& seg = strip.getSegment(i);
          if (seg.isActive()) {
            seg.blur(spatialBlurAmount);
          }
        }
      }

      if (motionBlurAmount > 0 && prevFrame != nullptr) {
        for (uint16_t i = 0; i < numLeds; i++) {
          uint32_t newColor = strip.getPixelColor(i);
          uint32_t blendedColor = blendColors(prevFrame[i], newColor, motionBlurAmount);
          
          strip.setPixelColor(i, blendedColor); 
          prevFrame[i] = blendedColor;          
        }
      } else if (prevFrame != nullptr) {
        for (uint16_t i = 0; i < numLeds; i++) {
          prevFrame[i] = strip.getPixelColor(i);
        }
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) top = root.createNestedObject("Global_Blur");
      top["enabled"] = enabled;
      top["spatial_amount"] = spatialBlurAmount;
      top["motion_amount"] = motionBlurAmount;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root["Global_Blur"];
      if (top.isNull()) return false;
      getJsonValue(top["enabled"], enabled);
      getJsonValue(top["spatial_amount"], spatialBlurAmount);
      getJsonValue(top["motion_amount"], motionBlurAmount);
      return true;
    }
    
    uint16_t getId() override {
      return 6969; 
    }
};

static GlobalBlurUsermod myBlurUsermod;
REGISTER_USERMOD(myBlurUsermod);
