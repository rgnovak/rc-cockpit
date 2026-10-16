/**************************************************************
 * RC-COCKPIT 2.0 - Core2350B - (2 DISPLAYS)
 * ============================================================
 * Waveshare Core2350B
 * 2x ST7735 Display (128x128):
 *   - Display 1 (links)  : Horizont (PFD)
 *   - Display 2 (rechts) : Karte (Map)
 * 1x Piloten-Servo
 * PWM-Eingang (3-Stufen-Schalter, TOGGLE-Logik):
 *   - Hochschalten   -> Kopfbewegung an/aus
 *   - Runterschalten -> beide Displays an/aus
 *
 * STARTZUSTAND: Beim Einschalten der Spannung bewegt sich der
 * Pilotenkopf nicht und beide Displays bleiben komplett dunkel
 * (kein Bootscreen, keine Ausgabe) - bis der Nutzer per Schalter
 * aktiv etwas einschaltet. Der Garmin-Bootscreen wird NICHT mehr
 * beim Booten gezeigt, sondern erst beim ERSTEN Einschalten der
 * Displays per Schalter (einmalig), danach direkt Horizont/Karte.
 * Spätere Aus-/Ein-Zyklen zeigen den Bootscreen nicht erneut.
 *
 * Abgeleitet von der funktionierenden 4-Display-Version.
 *************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Servo.h>

#include "imu.h"
#include "pfd.h"
#include "map.h"
#include "piloten.h"
#include "pwm_input.h"
#include "bootscreen.h"

#define W 128
#define H 128
#define CX 64
#define CY 64
#define FB_BYTES (W * H * 2)
#define TOP_TAPE_H 16
#define PANEL_XSTART 2
#define PANEL_YSTART 1

/* ================= SPI PINS ================= */
#define PIN_SPI_SCK   6    // SCL
#define PIN_SPI_MOSI  7    // SDA
#define PIN_CS1  8    // Display 1 (links)  - Horizont
#define PIN_CS2  9    // Display 2 (rechts) - Karte
#define PIN_DC   12
#define PIN_RST  13
#define PIN_BL1  14   // Backlight Display 1
#define PIN_BL2  15   // Backlight Display 2

/* ================= IMU (I2C) ================= */
#define PIN_IMU_SDA   4    // blau
#define PIN_IMU_SCL   5    // gelb

/* ================= PILOTENKOPF (SERVO) ================= */
#define PIN_PILOT_SERVO_1  21    // vorher auf 2

/* ================= PWM-EINGANG (3-Stufen-Schalter) =================
 * Frei gewordener Pin (war CS3 in der 4-Display-Version) - bereits
 * als Header-Pin auf diesem Board erprobt. (GP9 ist jetzt wieder
 * CS2 für das zweite Display, siehe oben.)
 *
 * TOGGLE-Logik (nicht level-basiert!): gewertet wird nur der
 * SCHALTVORGANG (Zonen-Wechsel), nicht die aktuelle Stellung.
 *   -> HOCHSCHALTEN (Wechsel in die HIGH-Zone, ~2000us):
 *      Kopfbewegung an/aus umschalten (Toggle)
 *   -> RUNTERSCHALTEN (Wechsel in die LOW-Zone, ~1000us):
 *      BEIDE Displays an/aus umschalten (Toggle)
 *   -> CENTER (~1500us) selbst löst nichts aus, ist nur
 *      Ruhe-/Durchgangsstellung.
 * Beide Zustände starten beim Boot auf "aus", unabhängig von der
 * Schalterstellung beim Einschalten (siehe Priming in setup()).
 * Das erste Einschalten der Displays löst zusätzlich einmalig den
 * Garmin-Bootscreen aus (siehe showBootscreenNow()).
 * ===================================================== */
#define PIN_SWITCH_IN  10

/* ================= BOOTSCREEN ================= */
#define BOOTSCREEN_DELAY_MS  4000  // Anzeigedauer in Millisekunden

uint16_t *fbA;
uint16_t *fbB;
uint16_t *frontFB;
uint16_t *backFB;

uint8_t activeCS = PIN_CS1;

bool displayEnabled = false;  // Startzustand beim Booten: AUS, kein Bootscreen, keine Ausgabe
bool backlightIsOn  = false;  // Spiegelt den aktuellen Backlight-Zustand (beide Displays gemeinsam)

/* Garmin-Bootscreen: wird NICHT mehr beim Booten gezeigt, sondern
 * erst beim allerersten Einschalten der Displays per Schalter
 * (siehe loop() / showBootscreenNow()), danach nie wieder. */
bool bootscreenShown    = false;  // wurde der Bootscreen seit dem Einschalten schon mal gezeigt?
bool showingBootscreen  = false;  // sind wir gerade (nicht-blockierend) in der Bootscreen-Anzeige?
unsigned long bootscreenUntilMs = 0;

/* Zonen-Merker für die Toggle-Erkennung des PWM-Schalters (siehe
 * loop()). Wird in setup() nach dem ersten echten Sample auf die
 * tatsächliche Schalterstellung beim Boot "geprimt", damit die
 * Ausgangsstellung nicht fälschlich als Schaltvorgang zählt - das
 * bleibt unabhängig vom An/Aus-Startzustand: es geht nur darum,
 * eine zufällige Schalterposition beim Einschalten nicht als
 * absichtliches Umschalten misszuverstehen. */
PwmZone lastZone = PWM_ZONE_CENTER;

/* ======================================================
 * DISPLAY LOW LEVEL
 * ====================================================== */
inline void writeCmd(uint8_t c){
  digitalWrite(PIN_DC, LOW);
  digitalWrite(activeCS, LOW);
  SPI.transfer(c);
  digitalWrite(activeCS, HIGH);
}

inline void writeData8(uint8_t d){
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(activeCS, LOW);
  SPI.transfer(d);
  digitalWrite(activeCS, HIGH);
}

/* ======================================================
 * ST7735 INIT
 * ====================================================== */
void initDisplay(){
  Serial.print("  Init Display CS");
  Serial.print(activeCS);
  Serial.print("...");

  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  writeCmd(0x01); delay(150);  // SWRESET
  writeCmd(0x11); delay(500);  // SLPOUT
  writeCmd(0x3A); writeData8(0x05);  // RGB565
  writeCmd(0x36); writeData8(0xC0);  // Rotation 180°
  writeCmd(0x29);  // DISPON

  SPI.endTransaction();

  Serial.println(" OK!");
}

/* ======================================================
 * DISPLAY SCHWARZ LÖSCHEN (ohne Framebuffer, direkt per SPI)
 * Für beide Displays beim Boot verwendet, bevor die
 * Backlights eingeschaltet werden.
 * ====================================================== */
void clearDisplayBlack(uint8_t cs){
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

  activeCS = cs;

  writeCmd(0x2A);
  writeData8(0); writeData8(PANEL_XSTART);
  writeData8(0); writeData8(PANEL_XSTART + W - 1);

  writeCmd(0x2B);
  writeData8(0); writeData8(PANEL_YSTART);
  writeData8(0); writeData8(PANEL_YSTART + H - 1);

  writeCmd(0x2C);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(activeCS, LOW);

  uint8_t black_hi = COL_BLACK >> 8;
  uint8_t black_lo = COL_BLACK & 0xFF;

  for (int i = 0; i < W * H; i++) {
    SPI.transfer(black_hi);
    SPI.transfer(black_lo);
    if (i % 512 == 0) yield();
  }
  digitalWrite(activeCS, HIGH);

  SPI.endTransaction();
}

/* ======================================================
 * PUSH FRAMEBUFFER (an das Display, das aktuell in activeCS steht)
 * ====================================================== */
void push(){
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

  writeCmd(0x2A);
  writeData8(0); writeData8(PANEL_XSTART);
  writeData8(0); writeData8(PANEL_XSTART + W - 1);

  writeCmd(0x2B);
  writeData8(0); writeData8(PANEL_YSTART);
  writeData8(0); writeData8(PANEL_YSTART + H - 1);

  writeCmd(0x2C);

  digitalWrite(PIN_DC, HIGH);
  digitalWrite(activeCS, LOW);

  uint8_t *ptr = (uint8_t*)frontFB;
  for(int i = 0; i < FB_BYTES; i++) {
    SPI.transfer(ptr[i]);
    if (i % 512 == 0) yield();
  }

  digitalWrite(activeCS, HIGH);
  SPI.endTransaction();
}

/* ======================================================
 * EINE ANSICHT RENDERN UND AUF EIN DISPLAY AUSGEBEN
 * ------------------------------------------------------
 * Zeichnet in backFB, swapt dann front/back, pusht das
 * fertige Bild an das per "cs" angegebene Display, und
 * interleaved zwei Servo-Updates dabei (vor/nach dem Push),
 * damit die Pilotenkopf-Bewegung weiterhin flüssig bleibt.
 * ====================================================== */
void renderHorizonAndPush(uint8_t cs, float pitch, float roll, int heading){
  clearFB(COL_BLACK);

  int bug     = (heading + 45) % 360;
  int course  = (heading + 30) % 360;
  int bearing = (heading + 90) % 360;
  int spd = 140;
  int alt = 6000;
  int vsi = 0;

  drawHorizon(pitch, roll);
  drawPitchLadder(pitch, roll);
  drawRollScale();
  drawCoursePointer(heading, course);
  drawBearingPointer(heading, bearing);
  drawCDIScale();
  drawCDI(course, bearing);
  drawToFromText(course, bearing);
  drawHeadingTape(heading, bug);
  drawHeadingWindow(heading);
  renderAirspeedTape(spd);
  drawSpeedWindow(spd);
  renderAltitudeTape(alt);
  drawAltitudeWindow(alt);
  drawVSI(vsi);
  drawVSINumber();
  drawAircraft();

  uint16_t *tmp = frontFB;
  frontFB = backFB;
  backFB  = tmp;
  setPFDFramebuffer(backFB);

  pilotenUpdate();  // Update vor dem Push

  activeCS = cs;
  push();

  pilotenUpdate();  // Update nach dem Push
}

void renderMapAndPush(uint8_t cs, int heading){
  clearFB(COL_BLACK);

  trackLat[trackHead] = ownLat;
  trackLon[trackHead] = ownLon;
  trackHead = (trackHead + 1) % TRACK_LEN;
  if(trackHead == 0) trackFilled = true;

  drawMapBackground();
  drawMapTrack(ownLat, ownLon, heading);
  drawMapHeadingArrow(heading);
  drawMapAircraftChevron(heading);

  uint16_t *tmp = frontFB;
  frontFB = backFB;
  backFB  = tmp;
  setPFDFramebuffer(backFB);

  pilotenUpdate();  // Update vor dem Push

  activeCS = cs;
  push();

  pilotenUpdate();  // Update nach dem Push
}

/* ======================================================
 * GARMIN BOOTSCREEN - EINMALIG BEIM ERSTEN EINSCHALTEN
 * ------------------------------------------------------
 * Wird aus loop() aufgerufen, sobald displayEnabled zum ersten
 * Mal seit dem Booten auf true wechselt (siehe dort). Zeichnet
 * den Bootscreen, pusht ihn auf beide Displays und schaltet die
 * Backlights ein. Die Anzeigedauer (BOOTSCREEN_DELAY_MS) wird
 * NICHT blockierend per delay() gehalten, sondern über
 * showingBootscreen/bootscreenUntilMs in loop() abgewartet,
 * damit Servo/IMU währenddessen normal weiterlaufen.
 * ====================================================== */
void showBootscreenNow(){
  Serial.println("\nZeige Garmin Bootscreen (erstmaliges Einschalten)...");

  showGarminBootscreen();  // zeichnet in backFB

  uint16_t *tmp = frontFB;
  frontFB = backFB;
  backFB  = tmp;

  activeCS = PIN_CS1; push();
  activeCS = PIN_CS2; push();

  // Framebuffer-Zeiger zurück in den Normalzustand, damit
  // nachfolgendes Rendern (Horizont/Karte) wieder in den
  // richtigen Puffer zeichnet - wie von den render*AndPush()
  // Funktionen erwartet.
  tmp = frontFB;
  frontFB = backFB;
  backFB  = tmp;
  setPFDFramebuffer(backFB);

  digitalWrite(PIN_BL1, HIGH);
  digitalWrite(PIN_BL2, HIGH);
  backlightIsOn = true;

  bootscreenShown   = true;
  showingBootscreen = true;
  bootscreenUntilMs = millis() + BOOTSCREEN_DELAY_MS;

  Serial.println("✓ Bootscreen angezeigt (wird non-blocking gehalten)\n");
}

/* ======================================================
 * SETUP
 * ====================================================== */
void setup(){
  Serial.begin(115200);
  delay(3000);

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  RC-COCKPIT 2.0 - RP2350 (2 DISPLAYS)  ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  Serial.println("Initialisiere Hardware...\n");

  pinMode(PIN_CS1, OUTPUT);
  pinMode(PIN_CS2, OUTPUT);
  pinMode(PIN_DC,  OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_BL1, OUTPUT);
  pinMode(PIN_BL2, OUTPUT);

  digitalWrite(PIN_CS1, HIGH);
  digitalWrite(PIN_CS2, HIGH);
  digitalWrite(PIN_BL1, LOW);   // Backlights AUS!
  digitalWrite(PIN_BL2, LOW);   // Backlights AUS!

  Serial.println("✓ Display-Pins konfiguriert");
  Serial.println("✓ Backlights vorerst AUS");

  SPI.setSCK(PIN_SPI_SCK);
  SPI.setTX(PIN_SPI_MOSI);
  SPI.begin();
  Serial.println("✓ SPI initialisiert");

  digitalWrite(PIN_RST, LOW);  delay(20);
  digitalWrite(PIN_RST, HIGH); delay(80);
  Serial.println("✓ Display Reset");

  Serial.println("\nInitialisiere Displays:");

  activeCS = PIN_CS1; initDisplay();
  activeCS = PIN_CS2; initDisplay();

  Serial.println("\n✓ Displays initialisiert!");

  // SOFORT nach Init: beide Displays schwarz machen (ohne Framebuffer!)
  Serial.println("Lösche Display-Inhalte...");
  clearDisplayBlack(PIN_CS1);
  clearDisplayBlack(PIN_CS2);
  Serial.println("✓ Displays gelöscht");

  // Backlights bleiben bewusst AUS - kein Bootscreen, keine
  // Ausgabe beim Booten. Der Bootscreen kommt erst beim ersten
  // Einschalten der Displays per Schalter (siehe loop()).
  Serial.println("✓ Backlights bleiben AUS (kein Bootscreen beim Booten)\n");

  fbA = (uint16_t*)malloc(FB_BYTES);
  fbB = (uint16_t*)malloc(FB_BYTES);

  if (!fbA || !fbB) {
    Serial.println("\n✗ FEHLER: Framebuffer-Allokierung!");
    while(1);
  }

  frontFB = fbA;
  backFB  = fbB;
  setPFDFramebuffer(backFB);

  Serial.println("✓ Framebuffer allokiert");

  displayEnabled = false;
  Serial.println("✓ Startzustand: Displays AUS, kein Bootscreen (wartet auf Schalter)");

  Wire.setSDA(PIN_IMU_SDA);
  Wire.setSCL(PIN_IMU_SCL);
  Wire.begin();

  imuBegin(PIN_IMU_SDA, PIN_IMU_SCL);
  Serial.println("✓ IMU initialisiert");

  pilotenBegin(PIN_PILOT_SERVO_1);
  pilotenSetEnabled(false);  // Startzustand: Kopfbewegung AUS (fährt/bleibt auf Neutral)
  Serial.println("✓ Piloten-Servo initialisiert (Startzustand: Kopfbewegung AUS)");

  pwmInputBegin(PIN_SWITCH_IN);
  Serial.println("✓ PWM-Eingang (Schalter) initialisiert");

  // Aktuelle Schalterstellung beim Boot einlesen und als Basis
  // "priming", damit sie NICHT als Schaltvorgang gewertet wird -
  // unabhängig davon, wo der Schalter beim Einschalten gerade
  // steht, zählt das NICHT als Umschalten. Displays/Kopfbewegung
  // starten (siehe oben) bewusst "aus" und werden erst durch eine
  // tatsächliche, spätere Schalterbewegung eingeschaltet.
  delay(50);
  pwmInputUpdate();
  lastZone = pwmInputGetZone();
  Serial.println("✓ Schalterstellung geprimt (Boot-Zone erfasst)");

  Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("✓✓✓ SETUP ABGESCHLOSSEN! ✓✓✓");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  Serial.println("Starte Main Loop...\n");
}

/* ======================================================
 * LOOP
 * ====================================================== */
void loop(){

  bool displayEnabledBefore = displayEnabled;  // für Bootscreen-Trigger (s.u.)

  /* ===== PWM-EINGANG AUSWERTEN (TOGGLE, nicht level-basiert) =====
   * Nur der Schaltvorgang (Zonen-Wechsel) löst etwas aus:
   *  - neu in HIGH-Zone  -> Kopfbewegung an/aus umschalten
   *  - neu in LOW-Zone   -> beide Displays an/aus umschalten
   *  - CENTER selbst tut nichts
   * Bei Signalverlust (Sender aus/Verbindung weg): Sicherheits-
   * Fallback, beides wieder an, Zonen-Merker zurücksetzen. */
  pwmInputUpdate();
  PwmZone zone = pwmInputGetZone();

  if (!pwmInputSignalPresent()) {
    displayEnabled = true;
    pilotenSetEnabled(true);
    lastZone = PWM_ZONE_CENTER;
  } else if (zone != lastZone) {
    if (zone == PWM_ZONE_HIGH) {
      pilotenSetEnabled(!pilotenGetEnabled());
    } else if (zone == PWM_ZONE_LOW) {
      displayEnabled = !displayEnabled;
    }
    lastZone = zone;
  }

  /* ===== ERSTES EINSCHALTEN DER DISPLAYS -> EINMALIG BOOTSCREEN =====
   * Egal ob per Schalter oder Signalverlust-Fallback ausgelöst:
   * wechselt displayEnabled zum allerersten Mal seit dem Booten
   * auf true, wird einmalig der Garmin-Bootscreen gezeigt. Jeder
   * weitere Aus-/Ein-Zyklus geht direkt zu Horizont/Karte. */
  if (displayEnabled && !displayEnabledBefore && !bootscreenShown) {
    showBootscreenNow();
  }

  /* Falls während der (non-blocking gehaltenen) Bootscreen-Anzeige
   * wieder ausgeschaltet wird: sofort abbrechen, Backlight-Sync
   * unten übernimmt das tatsächliche Abschalten. */
  if (showingBootscreen && !displayEnabled) {
    showingBootscreen = false;
  }

  /* Backlights passend zum Zustand schalten (nur bei Änderung, beide zusammen) */
  if (displayEnabled && !backlightIsOn) {
    digitalWrite(PIN_BL1, HIGH);
    digitalWrite(PIN_BL2, HIGH);
    backlightIsOn = true;
  } else if (!displayEnabled && backlightIsOn) {
    digitalWrite(PIN_BL1, LOW);
    digitalWrite(PIN_BL2, LOW);
    backlightIsOn = false;
  }

  /* ===== IMU / PILOTENKOPF (immer aktualisieren) ===== */
  imuUpdate();

  float pitch   = imuGetPitch();
  float roll    = imuGetRoll();
  int   heading = imuGetHeading();

  pilotenSetRoll(roll);
  pilotenUpdate();  // 1. Update

  if (!displayEnabled) {
    // Displays aus: nichts rendern/pushen, aber Loop weiter laufen
    // lassen, damit Servo/IMU flüssig bleiben.
    yield();
    return;
  }

  if (showingBootscreen) {
    // Bootscreen wird für BOOTSCREEN_DELAY_MS gehalten - nicht
    // blockierend, Servo/IMU liefen oben bereits normal weiter.
    if (millis() >= bootscreenUntilMs) {
      showingBootscreen = false;
    } else {
      yield();
      return;
    }
  }

  /* ===== BEIDE ANSICHTEN RENDERN - JE EIN EIGENES DISPLAY =====
   * Kein zeitgesteuerter Wechsel mehr nötig: Horizont läuft
   * dauerhaft auf Display 1 (links), Karte dauerhaft auf
   * Display 2 (rechts). */
  renderHorizonAndPush(PIN_CS1, pitch, roll, heading);
  renderMapAndPush(PIN_CS2, heading);

  // Kein delay - Loop läuft so schnell wie möglich, Servo bekommt
  // weiterhin mehrere Updates pro Frame.
}
