#pragma once
#include <Arduino.h>

/**************************************************************
 * PILOTEN GAZE MODULE – INTERFACE (REDUCED: 1 SERVO)
 *
 * Realistische, non-blockierende Pilot-/Blickbewegungen
 * (Servo-gesteuert)
 *
 * - ein Servo
 * - Zufallsbewegungen
 * - S-Kurven (Easing)
 * - per pilotenSetEnabled() ein-/ausschaltbar (fährt dann auf
 *   Neutralstellung und hält dort, statt sich weiterzubewegen)
 *************************************************************/

/* Initialisierung des Piloten-Servos */
void pilotenBegin(int pin1);

/* Zyklisches Update (im loop() aufrufen) */
void pilotenUpdate();

/* Rollwinkel setzen (Grad, z. B. -45 … +45) */
void pilotenSetRoll(float rollDeg);

/* Kopfbewegung an/aus (aus = Servo fährt auf Neutralstellung und hält) */
void pilotenSetEnabled(bool enabled);
bool pilotenGetEnabled();
