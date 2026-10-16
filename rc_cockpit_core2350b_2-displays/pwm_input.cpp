/**************************************************************
 * PWM INPUT MODULE – IMPLEMENTATION
 * ============================================================
 * Nicht-blockierende Pulsbreitenmessung per Pin-Change-Interrupt.
 *************************************************************/

#include "pwm_input.h"

/* ==========================================================
 * ZONEN-SCHWELLEN (mit Hysterese, us)
 * ========================================================== */
static const uint16_t LOW_ENTER  = 1300;  // < 1300us  -> LOW
static const uint16_t LOW_EXIT   = 1350;  // > 1350us  -> zurück Richtung CENTER
static const uint16_t HIGH_ENTER = 1700;  // > 1700us  -> HIGH
static const uint16_t HIGH_EXIT  = 1650;  // < 1650us  -> zurück Richtung CENTER

/* Kein gültiger Puls mehr seit dieser Zeit -> Signal als "weg"
 * behandeln und auf CENTER (Normalbetrieb) zurückfallen. */
static const unsigned long SIGNAL_TIMEOUT_MS = 250;

/* ==========================================================
 * ISR-SEITIGE MESSUNG
 * ========================================================== */
static volatile uint32_t isrRiseUs = 0;
static volatile uint16_t isrLastWidthUs = 0;
static volatile unsigned long isrLastEdgeMs = 0;
static volatile bool isrHaveNewSample = false;

static uint8_t pwmPin = 255;

static void pwmIsr() {
  uint32_t now = micros();
  if (digitalRead(pwmPin) == HIGH) {
    // Steigende Flanke: Startzeit merken
    isrRiseUs = now;
  } else {
    // Fallende Flanke: Pulsbreite berechnen
    uint32_t width = now - isrRiseUs;
    // Plausibilitäts-Check, Standard-RC-PWM: grob 800-2200us
    if (width >= 700 && width <= 2300) {
      isrLastWidthUs = (uint16_t)width;
      isrHaveNewSample = true;
      isrLastEdgeMs = millis();
    }
  }
}

/* ==========================================================
 * HAUPTSEITIGER ZUSTAND
 * ========================================================== */
static PwmZone currentZone = PWM_ZONE_CENTER;
static uint16_t lastGoodUs = 1500;
static bool signalPresent = false;
static unsigned long lastSampleMs = 0;

void pwmInputBegin(uint8_t pin) {
  pwmPin = pin;
  pinMode(pwmPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(pwmPin), pwmIsr, CHANGE);
}

void pwmInputUpdate() {
  uint16_t width;
  bool haveSample;
  unsigned long edgeMs;

  noInterrupts();
  haveSample = isrHaveNewSample;
  width = isrLastWidthUs;
  edgeMs = isrLastEdgeMs;
  isrHaveNewSample = false;
  interrupts();

  unsigned long now = millis();

  if (haveSample) {
    lastGoodUs = width;
    lastSampleMs = edgeMs;

    /* Zonen-Klassifikation mit Hysterese */
    switch (currentZone) {
      case PWM_ZONE_CENTER:
        if (width < LOW_ENTER)       currentZone = PWM_ZONE_LOW;
        else if (width > HIGH_ENTER) currentZone = PWM_ZONE_HIGH;
        break;
      case PWM_ZONE_LOW:
        if (width > LOW_EXIT)        currentZone = PWM_ZONE_CENTER;
        break;
      case PWM_ZONE_HIGH:
        if (width < HIGH_EXIT)       currentZone = PWM_ZONE_CENTER;
        break;
    }
  }

  /* Signal-Timeout: kein Puls mehr -> Normalbetrieb (CENTER) */
  if (lastSampleMs == 0 || (now - lastSampleMs) > SIGNAL_TIMEOUT_MS) {
    signalPresent = false;
    currentZone = PWM_ZONE_CENTER;
  } else {
    signalPresent = true;
  }
}

PwmZone pwmInputGetZone() {
  return currentZone;
}

uint16_t pwmInputGetRawUs() {
  return lastGoodUs;
}

bool pwmInputSignalPresent() {
  return signalPresent;
}
