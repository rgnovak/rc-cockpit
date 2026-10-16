#pragma once
#include <Arduino.h>

void setPFDFramebuffer(uint16_t* fb);

#define W 128
#define H 128
#define CX 64
#define CY 64
#define TOP_TAPE_H 16
#define PITCH_SCALE 2
#define CDI_PIXELS 10

inline uint16_t RGB565(uint8_t r,uint8_t g,uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/* ================== PANEL COLOR (nur für Laufzeit-Konvertierung) ================== */
inline uint16_t panelColor(uint16_t rgb){
  uint16_t r = (rgb >> 11) & 0x1F;
  uint16_t g = (rgb >> 5)  & 0x3F;
  uint16_t b =  rgb        & 0x1F;
  uint16_t bgr = (b << 11) | (g << 5) | r;
  uint16_t inv = ~bgr;
  return (inv << 8) | (inv >> 8);
}

/* ================== COLORS - VORKONVERTIERTE HEX-WERTE! ================== */
// Diese Werte sind BEREITS durch panelColor() gelaufen (wie map Bitmap!)
#define COL_BLACK    0xFFFF
#define COL_WHITE    0x0000
#define COL_GREEN    0x1FF8
#define COL_YELLOW   0x00F8
#define COL_MAGENTA  0xE007
#define COL_SKY      0x9533
#define COL_GND      0xD0E5
#define COL_DARKGRAY 0x75AD

extern uint16_t *backFB;

void px(int x,int y,uint16_t c);
void fillRect(int x,int y,int w,int h,uint16_t c);
void clearFB(uint16_t c);
void drawDigit(int x,int y,int d,uint16_t c);
void drawNumber(int x,int y,int n,uint16_t c);
void drawHorizon(float pitch,float roll);
void drawPitchLadder(float pitch,float roll);
void drawRollScale();
void drawAircraft();
void drawVSI(int vsi);
void drawVSINumber();
void renderAirspeedTape(int spd);
void drawSpeedWindow(int spd);
void renderAltitudeTape(int alt);
void drawAltitudeWindow(int alt);
void drawCDIScale();
void drawCDI(int course,int bearing);
void drawToFromText(int course,int bearing);
void drawCoursePointer(int heading,int course);
void drawBearingPointer(int heading,int bearing);
void drawHeadingTape(int heading,int bug);
void drawHeadingWindow(int heading);
