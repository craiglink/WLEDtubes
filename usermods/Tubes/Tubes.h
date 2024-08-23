#pragma once

#include "wled.h"

#include "util.h"
#include "options.h"

// #define USERADIO

#include "FX.h"

#include "virtual_strip.h"
#include "led_strip.h"
#include "master.h"

#include "controller.h"
#include "debug.h"


#define MASTER_PIN 25
#define LEGACY_PIN 32  // DigUno Q4

extern uint32_t savedStrip[];


class TubesUsermod : public Usermod {
  private:
    PatternController controller = PatternController();
    DebugController debug = DebugController(controller);
    Master master = Master(controller);
    bool isLegacy = false;

    void randomize() {
      randomSeed(esp_random());
      random16_set_seed(random(0, 65535));
      random16_add_entropy(esp_random());
    }

  public:
    void setup() {

      if (pinManager.isPinOk(MASTER_PIN)) {
        pinMode(MASTER_PIN, INPUT_PULLUP);
        if(pinManager.isPinOk(LEGACY_PIN)) {
          pinMode(LEGACY_PIN, INPUT_PULLUP);
        }
        if (digitalRead(MASTER_PIN) == LOW) {
        }
        isLegacy = (digitalRead(MASTER_PIN) == LOW);
      }
      randomize();

      // Override some behaviors on all Tubes
      bootPreset = 0;  // Try to prevent initial playlists from starting
      fadeTransition = true;  // Fade palette transitions
      transitionDelay = 8000;   // Fade them for a long time
      strip.setTargetFps(60);
      strip.setCCT(100);

      // Start timing
      globalTimer.setup();
      controller.setup();
      if (controller.isMasterRole()) {
        master.setup();
      }
      debug.setup();
    }

    void loop()
    {
      EVERY_N_MILLISECONDS(10000) {
        randomize();
      }

      globalTimer.update();

      if (controller.isMasterRole()) {
        master.update();
      }
      controller.update();
      debug.update();

      // Draw after everything else is done
      controller.led_strip.update();
    }

    void handleOverlayDraw() {
      // Draw effects layers over whatever WLED is doing.
      controller.handleOverlayDraw();
      debug.handleOverlayDraw();
      if (controller.isMasterRole()) {
        master.handleOverlayDraw();
      }

    #ifdef LED_MASK_OFFSET
      uint16_t len = strip.getLengthTotal();
      uint16_t savedLen = std::min(len, static_cast<uint16_t>(MAX_SAVED_LEDS));
      static_assert( LED_MASK_OFFSET < 0xffff, "LED_MASK_OFFSET set to large");
      uint16_t maskLength = std::min(len, static_cast<uint16_t>(LED_MASK_OFFSET));
      // save existing strip
      for (auto i = 0; i < savedLen; i++) {
        savedStrip[i] = strip.getPixelColor(i);
      }
      // shift all colors to beginning of strip by mask length
      uint16_t p = 0;
      for (auto i = maskLength; i < len; i++) {
        strip.setPixelColor(p++, strip.getPixelColor(i));
      }
      // black out the remaining
      for ( ; p < len; p++) {
          strip.setPixelColor(p, CRGB::Black);
      }
    #endif


      // When AP mode is on, make sure it's obvious
      // Blink when there's a connected client
      if (apActive) {
        strip.setPixelColor(0, CRGB::Purple);
        if (millis() % 4000 > 1000 && WiFi.softAPgetStationNum()) {
          strip.setPixelColor(0, CRGB::Black);
        }
        strip.setPixelColor(1, CRGB::Black);
      }
    }

    bool handleButton(uint8_t b) {
      // Special code for handling the "power save" button
      if (b == 100) { // Press button 0 for WLED_LONG_POWER_SAVE ms
        controller.togglePowerSave();
        return true;
      }
      if (b == 101) { // Short press button 0 (piggybacks with default)
        controller.cancelOverrides();
        return true;
      }
      if (b == 102) { // Double-click button 0
        controller.acknowledge();
        if (controller.isSelecting()) {
          if (controller.isSelected())
            controller.deselect();
          else
            controller.select();
        } else {
          controller.request_new_bpm();
        }
        return true;
      }

      return false;
    }

};
