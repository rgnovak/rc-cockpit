/**************************************************************
 * PILOTEN MODULE – IMPLEMENTATION (RP2350, REDUCED: 1 SERVO)
 *
 * Raspberry Pi Pico 2 (RP2350) - Arduino-Pico Core
 * ------------------------------------------------------------
 * - Ein Servo (Pilotenkopf / Blickrichtung)
 * - Realistische Zufallsbewegungen
 * - Roll-abhängiger Bias (Pilot schaut in Kurvenrichtung)
 * - Non-blocking FSM
 * - Weiche Bewegungen (S-Kurve / Easing)
 * - Per pilotenSetEnabled(false) abschaltbar: Servo fährt auf
 *   Neutralstellung (90°, ohne Roll-Bias) und hält dort, die
 *   Zufalls-FSM pausiert. pilotenSetEnabled(true) nimmt die FSM
 *   ab der aktuellen Position wieder auf.
 *
 * TECHNIK:
 * - Arduino Servo Library (RP2350 PWM-Hardware)
 * - 50 Hz Standard-Servo-PWM
 * - Hardware PWM via RP2350 PWM-Slices
 *************************************************************/

#include "piloten.h"
#include <Arduino.h>
#include <Servo.h>
#include <math.h>

/* ==========================================================
 * SERVO GRUNDKONFIGURATION
 * ========================================================== */
static const int SERVO_MIN = 10;      // mechanisches Minimum (Grad)
static const int SERVO_MAX = 160;     // mechanisches Maximum (Grad)

/* Bewegungsparameter */
static const float SPEED_MIN = 8.0f;     // ms pro Grad (schnell)
static const float SPEED_MAX = 32.0f;    // ms pro Grad (langsam)

static const int PAUSE_MIN = 600;         // ms
static const int PAUSE_MAX = 2400;        // ms

/* ==========================================================
 * SERVO OBJEKT (RP2350 Servo Library)
 * ========================================================== */
static Servo servo1;

/* ==========================================================
 * SERVO CONTROLLER
 * ========================================================== */
struct ServoController {
  Servo *servo;                // Pointer auf Servo-Objekt
  float currentAngle;
  float startAngle;
  float targetAngle;
  float msPerDegree;
  unsigned long startTime;
  unsigned long duration;
  bool moving;
};

static ServoController SC1;

/* ==========================================================
 * ROLL-INPUT (vom IMU-Modul)
 * ========================================================== */
static float pilotRollDeg = 0.0f;

/* Wird aus der .ino gesetzt */
void pilotenSetRoll(float rollDeg){
  pilotRollDeg = constrain(rollDeg, -45.0f, 45.0f);
}

/* ==========================================================
 * ENABLE / DISABLE
 * ========================================================== */
static bool headEnabled = true;

void pilotenSetEnabled(bool enabled){
  headEnabled = enabled;
}

bool pilotenGetEnabled(){
  return headEnabled;
}

/* ==========================================================
 * HILFSFUNKTIONEN
 * ========================================================== */
static float ease(float t){
  // Smoothstep / S-Kurve
  return t * t * (3.0f - 2.0f * t);
}

/* Roll → Blick-Bias */
static int rollBias(){
  const float MAX_BIAS = 12.0f;   // max. Servo-Grad durch Roll
  return (int)(pilotRollDeg / 45.0f * MAX_BIAS);
}

/* ==========================================================
 * SERVO WRITE (RP2350 Servo Library)
 * ========================================================== */
static void servoWrite(Servo *servo, int angle){
  angle = constrain(angle, SERVO_MIN, SERVO_MAX);
  servo->write(angle);
}

/* ==========================================================
 * BEWEGUNG STARTEN
 * ========================================================== */
static void moveTo(ServoController &SC, int angle){
  angle = constrain(angle, SERVO_MIN, SERVO_MAX);

  if (!isfinite(SC.currentAngle)) {
    SC.currentAngle = angle;
  }

  SC.startAngle  = SC.currentAngle;
  SC.targetAngle = angle;

  float delta = fabs(SC.targetAngle - SC.startAngle);

  SC.msPerDegree = random((int)(SPEED_MIN * 100),
                          (int)(SPEED_MAX * 100)) / 100.0f;

  SC.duration = max((unsigned long)(delta * SC.msPerDegree),
                    (unsigned long)80);

  SC.startTime = millis();
  SC.moving = true;
}

/* ==========================================================
 * BEWEGUNG UPDATE (NON-BLOCKING)
 * ========================================================== */
static void updateMovement(ServoController &SC){
  if (!SC.moving) return;

  unsigned long elapsed = millis() - SC.startTime;

  if (elapsed >= SC.duration) {
    servoWrite(SC.servo, (int)SC.targetAngle);
    SC.currentAngle = SC.targetAngle;
    SC.moving = false;
    return;
  }

  float t = (float)elapsed / (float)SC.duration;
  float pos = SC.startAngle +
              (SC.targetAngle - SC.startAngle) * ease(t);

  servoWrite(SC.servo, (int)pos);
  SC.currentAngle = pos;
}

/* ==========================================================
 * NEUTRALPOSITIONEN
 * ========================================================== */
static int neutral1 = 90;

static void updateNeutral(int &neutral){
  int base = 90 + rollBias();
  neutral = constrain(base + random(-5, 6),
                      SERVO_MIN + 10,
                      SERVO_MAX - 10);
}

/* ==========================================================
 * FSM STATUS
 * ========================================================== */
static bool seqRunning1 = false;

static int seqStage1 = 0;

static unsigned long seqWaitUntil1 = 0;

static int seqA1, seqB1;

static unsigned long nextAction1 = 0;

/* Merkt, ob wir gerade (durch Disable) auf dem Weg zur reinen
 * Neutralstellung sind, um moveTo() nicht jeden Loop neu
 * anzustoßen. */
static bool parking = false;

/* ==========================================================
 * BLICK-SEQUENZ
 * ========================================================== */
static void startGazeSequence(ServoController &SC,
                              int &neutral,
                              int &seqStage,
                              bool &seqRunning,
                              int &tA,
                              int &tB)
{
  int left  = constrain(neutral - random(15, 40), SERVO_MIN, SERVO_MAX);
  int right = constrain(neutral + random(15, 40), SERVO_MIN, SERVO_MAX);

  if (random(2) == 0) { tA = left;  tB = right; }
  else               { tA = right; tB = left;  }

  seqStage = 1;
  seqRunning = true;
}

/* ==========================================================
 * PUBLIC API
 * ========================================================== */
void pilotenBegin(int pin1){

  /* Servo Library initialisieren */
  servo1.attach(pin1);

  /* Controller initialisieren */
  SC1 = { &servo1, NAN, 0, 0, 0, 0, 0, false };

  updateNeutral(neutral1);

  moveTo(SC1, neutral1);

  nextAction1 = millis() + 300;
}

void pilotenUpdate(){

  unsigned long now = millis();

  updateMovement(SC1);

  if (!headEnabled) {
    /* Abgeschaltet: einmalig auf reine Neutralstellung (90°, ohne
     * Roll-Bias) fahren und dort halten. Laufende Blicksequenz/FSM
     * wird eingefroren, nicht fortgesetzt. */
    if (!parking) {
      seqRunning1 = false;
      moveTo(SC1, 90);
      parking = true;
    }
    return;
  }

  if (parking) {
    /* Gerade wieder aktiviert: FSM ab jetzt frisch starten. */
    parking = false;
    nextAction1 = now + random(PAUSE_MIN, PAUSE_MAX);
  }

  /* ---------------- PILOT ---------------- */
  if (!seqRunning1 && now >= nextAction1) {

    nextAction1 = now + random(PAUSE_MIN, PAUSE_MAX);

    if (random(100) < 25) {
      startGazeSequence(SC1, neutral1,
                        seqStage1, seqRunning1,
                        seqA1, seqB1);
    } else {
      moveTo(SC1, random(SERVO_MIN, SERVO_MAX));
      seqStage1 = 10;
      seqRunning1 = true;
    }
  }

  if (seqRunning1) {
    if (seqStage1 == 1 && !SC1.moving) {
      moveTo(SC1, seqA1);
      seqStage1 = 2;
      seqWaitUntil1 = now + random(150, 350);
    }
    else if (seqStage1 == 2 && !SC1.moving && now >= seqWaitUntil1) {
      moveTo(SC1, seqB1);
      seqStage1 = 3;
      seqWaitUntil1 = now + random(150, 350);
    }
    else if ((seqStage1 == 3 || seqStage1 == 10) && !SC1.moving) {
      updateNeutral(neutral1);
      moveTo(SC1, neutral1);
      seqRunning1 = false;
    }
  }
}
