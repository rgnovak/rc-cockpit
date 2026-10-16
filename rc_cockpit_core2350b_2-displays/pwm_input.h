#pragma once
#include <Arduino.h>

/**************************************************************
 * PWM INPUT MODULE – INTERFACE
 * ============================================================
 * Liest einen Standard-RC-PWM-Kanal (1000-2000us, z.B. von einem
 * 3-Stufen-Schalter am Empfänger) nicht-blockierend per
 * Pin-Change-Interrupt aus (KEIN pulseIn()! Das würde den
 * Display-/Servo-Loop blockieren).
 *
 * Klassifiziert den gemessenen Pulse in drei Zonen:
 *   LOW    (~1000us, Schalter "runter")
 *   CENTER (~1500us, Mittelstellung - Normalbetrieb)
 *   HIGH   (~2000us, Schalter "hoch")
 * mit Hysterese an den Zonengrenzen, damit es nicht am Rand
 * flattert.
 *
 * Erkennt Signalverlust (kein Empfänger/keine Verbindung) und
 * fällt dann auf CENTER zurück (= Normalbetrieb, nichts abgeschaltet).
 *
 * VERWENDUNG:
 *   pwmInputBegin(PIN_SWITCH_IN);   // einmalig in setup()
 *   pwmInputUpdate();               // zyklisch in loop() (Timeout-Check)
 *   PwmZone z = pwmInputGetZone();
 *************************************************************/

enum PwmZone {
  PWM_ZONE_LOW,
  PWM_ZONE_CENTER,
  PWM_ZONE_HIGH
};

void pwmInputBegin(uint8_t pin);
void pwmInputUpdate();          // zyklisch aufrufen (prüft Signal-Timeout)
PwmZone pwmInputGetZone();
uint16_t pwmInputGetRawUs();    // letzter gültiger Pulse in us (Debug)
bool pwmInputSignalPresent();   // false = seit >250ms kein Puls mehr
