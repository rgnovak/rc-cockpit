/**************************************************************
 * imu.cpp - PIMORONI ICM20948 INTERFACE (RP2350 VERSION)
 * ============================================================
 * 
 * ÄNDERUNG FÜR RP2350:
 * - Wire.begin(sda, scl) funktioniert NICHT
 * - Stattdessen: Wire.setSDA() / Wire.setSCL() / Wire.begin()
 * 
 * Diese Datei ersetzt die Original imu.cpp!
 *************************************************************/

#include "imu.h"
#include <Wire.h>
#include <ICM_20948.h>

/* ====================================================
 * GLOBALE OBJEKTE
 * ==================================================== */
ICM_20948_I2C myICM;

/* ====================================================
 * SENSOR-WERTE (AKTUELL)
 * ==================================================== */
static float gPitch = 0.0f;
static float gRoll = 0.0f;
static int   gHeading = 0;
static float gSlip = 0.0f;

/* ====================================================
 * COMPLEMENTARY FILTER (Pitch/Roll)
 * ==================================================== */
static const float ALPHA = 0.98f;
static unsigned long lastUpdateTime = 0;

/* ====================================================
 * MAGNETOMETER KALIBRIERUNG
 * ==================================================== */
static float magOffsetX = 0.0f;
static float magOffsetY = 0.0f;
static float magOffsetZ = 0.0f;

/* ====================================================
 * HEADING FILTER (für sanfte 0°/360° Übergänge)
 * ==================================================== */
static float headingFiltered = 0.0f;
static const float HEADING_ALPHA = 0.85f;

/* ====================================================
 * HILFSFUNKTIONEN
 * ==================================================== */
static float wrapAngle(float angle) {
  while (angle > 180.0f) angle -= 360.0f;
  while (angle < -180.0f) angle += 360.0f;
  return angle;
}

static int wrapHeading(int h) {
  while (h < 0) h += 360;
  while (h >= 360) h -= 360;
  return h;
}

/* ====================================================
 * TILT-KOMPENSIERTES HEADING
 * ==================================================== */
static float getTiltCompensatedHeading(float magX, float magY, float magZ,
                                       float pitch, float roll)
{
  float pitchRad = pitch * DEG_TO_RAD;
  float rollRad = roll * DEG_TO_RAD;
  
  float cosPitch = cos(pitchRad);
  float sinPitch = sin(pitchRad);
  float cosRoll = cos(rollRad);
  float sinRoll = sin(rollRad);
  
  float Xh = magX * cosPitch + magZ * sinPitch;
  float Yh = magX * sinRoll * sinPitch + magY * cosRoll - magZ * sinRoll * cosPitch;
  
  float heading = atan2(Yh, Xh) * RAD_TO_DEG;
  
  if (heading < 0) heading += 360.0f;
  
  return heading;
}

/* ====================================================
 * MAGNETOMETER KALIBRIERUNG (EINFACH)
 * ==================================================== */
static void calibrateMag() {
  Serial.println("Magnetometer-Kalibrierung...");
  Serial.println("Bitte Board langsam um alle Achsen drehen!");
  
  float minX = 10000, maxX = -10000;
  float minY = 10000, maxY = -10000;
  float minZ = 10000, maxZ = -10000;
  
  for (int i = 0; i < 200; i++) {
    if (myICM.dataReady()) {
      myICM.getAGMT();
      
      float mx = myICM.magX();
      float my = myICM.magY();
      float mz = myICM.magZ();
      
      if (mx < minX) minX = mx;
      if (mx > maxX) maxX = mx;
      if (my < minY) minY = my;
      if (my > maxY) maxY = my;
      if (mz < minZ) minZ = mz;
      if (mz > maxZ) maxZ = mz;
    }
    delay(50);
  }
  
  magOffsetX = (maxX + minX) / 2.0f;
  magOffsetY = (maxY + minY) / 2.0f;
  magOffsetZ = (maxZ + minZ) / 2.0f;
  
  Serial.println("Kalibrierung fertig!");
  Serial.print("Offsets: X=");
  Serial.print(magOffsetX);
  Serial.print(" Y=");
  Serial.print(magOffsetY);
  Serial.print(" Z=");
  Serial.println(magOffsetZ);
}

/* ====================================================
 * INITIALISIERUNG (RP2350 VERSION!)
 * ==================================================== */
bool imuBegin(int pinSDA, int pinSCL)
{
  Serial.println("\n=== IMU INITIALISIERUNG (RP2350) ===");
  
  /* -------- I2C (RP2350 Methode!) -------- */
  Wire.setSDA(pinSDA);  // RP2350: Erst Pins setzen!
  Wire.setSCL(pinSCL);
  Wire.begin();         // RP2350: Dann begin() ohne Parameter!
  Wire.setClock(400000);  // 400 kHz
  delay(100);
  
  Serial.println("I2C Bus initialisiert (RP2350)");
  
  /* -------- ICM-20948 verbinden -------- */
  // Versuche Adresse 0x68 (Standard)
  myICM.begin(Wire, 0);
  
  if (myICM.status != ICM_20948_Stat_Ok) {
    // Versuche alternative Adresse 0x69
    myICM.begin(Wire, 1);
    
    if (myICM.status != ICM_20948_Stat_Ok) {
      Serial.println("FEHLER: ICM-20948 nicht gefunden!");
      Serial.print("Status: ");
      Serial.println(myICM.statusString());
      return false;
    }
  }
  
  Serial.println("ICM-20948 verbunden!");
  
  /* -------- Magnetometer aktivieren -------- */
  bool magOK = myICM.startupMagnetometer();
  if (!magOK) {
    Serial.println("WARNUNG: Magnetometer-Init fehlgeschlagen");
  } else {
    Serial.println("Magnetometer aktiviert");
  }
  
  /* -------- Kalibrierung -------- */
  // Optional: Auskommentieren wenn nicht benötigt
  // calibrateMag();
  
  /* -------- Filter initialisieren -------- */
  lastUpdateTime = millis();
  
  Serial.println("=== IMU BEREIT ===\n");
  return true;
}

/* ====================================================
 * UPDATE (SENSOR-FUSION)
 * ==================================================== */
void imuUpdate()
{
  if (!myICM.dataReady()) {
    return;
  }
  
  myICM.getAGMT();
  
  /* -------- Zeit-Delta -------- */
  unsigned long now = millis();
  float dt = (now - lastUpdateTime) / 1000.0f;
  lastUpdateTime = now;
  
  /* -------- Sensor-Daten (180° Montage-Korrektur) -------- */
  float ax = -myICM.accX();
  float ay = myICM.accY();   // NICHT negieren - sonst ist Pitch invertiert!
  float az = -myICM.accZ();
  
  float gx = -myICM.gyrX();
  float gy = myICM.gyrY();   // NICHT negieren - sonst ist Pitch invertiert!
  float gz = -myICM.gyrZ();
  
  float mx = -myICM.magX() - magOffsetX;
  float my = -myICM.magY() - magOffsetY;
  float mz = -myICM.magZ() - magOffsetZ;
  
  /* -------- Pitch/Roll aus Beschleunigung -------- */
  float pitchAcc = atan2(ay, sqrt(ax * ax + az * az)) * RAD_TO_DEG;
  float rollAcc = atan2(-ax, az) * RAD_TO_DEG;
  
  /* -------- Pitch/Roll aus Gyro (Integration) -------- */
  float pitchGyro = gPitch + gy * dt;
  float rollGyro = gRoll + gx * dt;
  
  /* -------- Complementary Filter -------- */
  gPitch = ALPHA * pitchGyro + (1.0f - ALPHA) * pitchAcc;
  gRoll = ALPHA * rollGyro + (1.0f - ALPHA) * rollAcc;
  
  /* -------- Tilt-kompensiertes Heading -------- */
  float heading = getTiltCompensatedHeading(mx, my, mz, gPitch, gRoll);
  
  /* -------- Heading Filter (sanfte 0°/360° Übergänge) -------- */
  float delta = heading - headingFiltered;
  if (delta > 180.0f) delta -= 360.0f;
  if (delta < -180.0f) delta += 360.0f;
  
  headingFiltered += delta * (1.0f - HEADING_ALPHA);
  if (headingFiltered < 0) headingFiltered += 360.0f;
  if (headingFiltered >= 360.0f) headingFiltered -= 360.0f;
  
  gHeading = (int)(headingFiltered + 0.5f);
  gHeading = wrapHeading(gHeading);
  
  /* -------- Slip Indikator -------- */
  gSlip = constrain(gz / 100.0f, -1.0f, 1.0f);
}

/* ====================================================
 * GETTER
 * ==================================================== */
float imuGetPitch() {
  return gPitch;
}

float imuGetRoll() {
  return gRoll;
}

int imuGetHeading() {
  return gHeading;
}

float imuGetSlip() {
  return gSlip;
}

/* ====================================================
 * DEBUG
 * ==================================================== */
void imuPrintDebug() {
  Serial.print("P: ");
  Serial.print(gPitch, 1);
  Serial.print("° R: ");
  Serial.print(gRoll, 1);
  Serial.print("° H: ");
  Serial.print(gHeading);
  Serial.print("° S: ");
  Serial.print(gSlip, 2);
  
  Serial.print(" | Mag: X=");
  Serial.print(myICM.magX() - magOffsetX, 1);
  Serial.print(" Y=");
  Serial.print(myICM.magY() - magOffsetY, 1);
  Serial.print(" Z=");
  Serial.println(myICM.magZ() - magOffsetZ, 1);
}

void imuPrintRaw() {
  Serial.print("Acc: ");
  Serial.print(myICM.accX(), 2);
  Serial.print(", ");
  Serial.print(myICM.accY(), 2);
  Serial.print(", ");
  Serial.print(myICM.accZ(), 2);
  
  Serial.print(" | Gyr: ");
  Serial.print(myICM.gyrX(), 2);
  Serial.print(", ");
  Serial.print(myICM.gyrY(), 2);
  Serial.print(", ");
  Serial.print(myICM.gyrZ(), 2);
  
  Serial.print(" | Mag: ");
  Serial.print(myICM.magX(), 2);
  Serial.print(", ");
  Serial.print(myICM.magY(), 2);
  Serial.print(", ");
  Serial.println(myICM.magZ(), 2);
}
