#pragma once
/**************************************************************
 * imu.h - PIMORONI ICM20948 INTERFACE
 * ============================================================
 * 
 * HARDWARE:
 * - Pimoroni ICM20948 9DoF (PIM448)
 * - SparkFun ICM-20948 Library
 * - Montage: 180Â° gedreht, Z-Achse nach oben
 * 
 * FEATURES:
 * - Tilt-kompensiertes Magnetometer-Heading (driftfrei)
 * - Complementary Filter fÃ¼r Pitch/Roll
 * - Automatische 180Â° Montage-Korrektur
 * - Slip/Turn Indikator
 * 
 * OUTPUT:
 * - Pitch:   -90Â° bis +90Â° (Nase hoch = positiv)
 * - Roll:    -180Â° bis +180Â° (rechts = positiv)
 * - Heading: 0Â° bis 360Â° (Nord = 0Â°, im Uhrzeigersinn)
 * - Slip:    -1.0 bis +1.0 (normiert)
 * 
 * VERWENDUNG:
 *   imuBegin(38, 39);     // Einmalig im setup()
 *   imuUpdate();          // Zyklisch im loop()
 *   
 *   float pitch = imuGetPitch();
 *   float roll = imuGetRoll();
 *   int heading = imuGetHeading();
 *   float slip = imuGetSlip();
 *************************************************************/

#include <Arduino.h>

/* ==========================================================
 * INITIALISIERUNG
 * ========================================================== */

/**
 * @brief Initialisiert die IMU
 * 
 * @param pinSDA I2C Data Pin
 * @param pinSCL I2C Clock Pin
 * @return true wenn erfolgreich, false bei Fehler
 * 
 * ABLAUF:
 * - I2C-Bus initialisieren
 * - ICM-20948 verbinden
 * - Magnetometer aktivieren
 * - Filter zurÃ¼cksetzen
 */
bool imuBegin(int pinSDA, int pinSCL);


/* ==========================================================
 * UPDATE
 * ========================================================== */

/**
 * @brief Liest Sensoren und aktualisiert Filter
 * 
 * MUSS regelmÃ¤ÃŸig aufgerufen werden (50-200 Hz)
 * 
 * AUFGABEN:
 * - Sensordaten auslesen
 * - Montage-Korrektur (180Â° Drehung)
 * - Pitch/Roll berechnen (Complementary Filter)
 * - Heading berechnen (Tilt-kompensiert)
 * - Slip berechnen
 */
void imuUpdate();


/* ==========================================================
 * GETTER
 * ========================================================== */

/**
 * @brief Liefert aktuellen Pitch-Winkel
 * @return Pitch in Grad (-90 bis +90)
 * 
 * KONVENTION:
 * - Positiv = Nase hoch
 * - Negativ = Nase runter
 */
float imuGetPitch();

/**
 * @brief Liefert aktuellen Roll-Winkel
 * @return Roll in Grad (-180 bis +180)
 * 
 * KONVENTION:
 * - Positiv = Rechts kippen
 * - Negativ = Links kippen
 */
float imuGetRoll();

/**
 * @brief Liefert aktuelles Heading
 * @return Heading in Grad (0 bis 359)
 * 
 * KONVENTION:
 * - 0Â°   = Nord
 * - 90Â°  = Ost
 * - 180Â° = SÃ¼d
 * - 270Â° = West
 * 
 * EIGENSCHAFTEN:
 * - Driftfrei (Magnetometer-basiert)
 * - Tilt-kompensiert
 * - Wrap-safe gefiltert
 */
int imuGetHeading();

/**
 * @brief Liefert Slip/Turn Indikator
 * @return Slip normiert (-1.0 bis +1.0)
 * 
 * VERWENDUNG:
 * - Slip Ball / Turn Coordinator
 * - 0.0 = koordinierte Kurve
 * - Â±1.0 = scharfe Kurve (~100Â°/s)
 */
float imuGetSlip();


/* ==========================================================
 * DEBUG (OPTIONAL)
 * ========================================================== */

/**
 * @brief Gibt IMU-Status Ã¼ber Serial aus
 * 
 * AUSGABE:
 * P: +12.3Â° R: -5.6Â° H: 287Â° S: +0.12 | Mag: X=23.4 Y=-12.8 Z=45.2
 */
void imuPrintDebug();

/**
 * @brief Gibt Sensor-Rohwerte aus
 */
void imuPrintRaw();