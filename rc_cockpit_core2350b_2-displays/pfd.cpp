#include "pfd.h"
#include <math.h>

/**************************************************************
 * PFD MODULE â€“ IMPLEMENTATION
 *************************************************************/

/* ================== FRAMEBUFFER ================== */
/* interner Framebuffer-Zeiger */
static uint16_t* pfdFB = nullptr;

/* wird von cockpit.ino gesetzt */
void setPFDFramebuffer(uint16_t* fb){
  pfdFB = fb;
}
/* ================== FRAMEBUFFER HELPERS ================== */
void px(int x,int y,uint16_t c){
  if((unsigned)x < W && (unsigned)y < H)
    backFB[y*W + x] = c;
}

void fillRect(int x,int y,int w,int h,uint16_t c){
  for(int yy=0; yy<h; yy++)
    for(int xx=0; xx<w; xx++)
      px(x+xx, y+yy, c);
}

void clearFB(uint16_t c){
  for(int i=0; i < W*H; i++)
    backFB[i] = c;
}

/* ================== MINI FONT 3x5 ================== */
static const uint8_t font3x5[10][5] = {
  {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},
  {5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},
  {7,5,7,5,7},{7,5,7,1,7}
};

void drawDigit(int x,int y,int d,uint16_t c){
  if(d < 0 || d > 9) return;

  for(int r=0; r<5; r++)
    for(int b=0; b<3; b++)
      if(font3x5[d][r] & (1 << (2-b)))
        px(x+b, y+r, c);
}

void drawNumber(int x,int y,int n,uint16_t c){
  n = abs(n);
  drawDigit(x,   y, (n/100)%10, c);
  drawDigit(x+4, y, (n/10)%10 , c);
  drawDigit(x+8, y,  n%10     , c);
}

/* ================== ATTITUDE ================== */
void drawHorizon(float pitch,float roll){
  float r = roll * DEG_TO_RAD;
  int py  = CY + pitch * PITCH_SCALE;

  for(int x=0; x<W; x++){
    int y = py + tanf(r) * (x - CX);
    y = constrain(y, TOP_TAPE_H, H);

    for(int yy = TOP_TAPE_H; yy < y; yy++)
      px(x, yy, COL_SKY);

    for(int yy = y; yy < H; yy++)
      px(x, yy, COL_GND);
  }
}

void drawPitchLadder(float pitch,float roll){
  float r = roll * DEG_TO_RAD;

  for(int p = -30; p <= 30; p += 5){
    if(p == 0) continue;

    int py = CY + (pitch - p) * PITCH_SCALE;
    int hw = (p % 10 == 0) ? 26 : 14;

    for(int x = -hw; x <= hw; x++){
      int yy = py + tanf(r) * x;
      if(yy >= TOP_TAPE_H && yy < H)
        px(CX + x, yy, COL_WHITE);
    }
  }
}

void drawRollScale(){
  for(int i = -30; i <= 30; i += 10)
    px(CX + i, TOP_TAPE_H + 2, COL_WHITE);
}

void drawAircraft(){
  for(int i=-20;i<=20;i++) px(CX+i, CY, COL_YELLOW);
  for(int i=0;i<=10;i++)   px(CX,   CY+i, COL_YELLOW);
}

/* ================== VSI ================== */
static int vsiF = 0;

void drawVSI(int vsi){
  const int y   = H - 8;
  const int x0  = 24;
  const int w   = W - 48;
  const int mid = x0 + w/2;

  vsi = constrain(vsi, -2000, 2000);
  vsiF += (vsi - vsiF) >> 3;

  fillRect(x0-2, y-3, w+4, 6, COL_BLACK);

  px(x0,   y, COL_WHITE);
  px(mid,  y, COL_WHITE);
  px(x0+w, y, COL_WHITE);

  int dx = map(vsiF, -2000, 2000, -w/2, w/2);

  if(vsiF >= 0)
    fillRect(mid,    y-2, dx,  4, COL_GREEN);
  else
    fillRect(mid+dx, y-2, -dx, 4, COL_GREEN);
}

void drawVSINumber(){
  const int y  = H - 2;
  const int x0 = CX - 12;

  int v = vsiF / 10;
  v = constrain(v, -999, 999);

  fillRect(x0-2, y-8, 28, 8, COL_BLACK);

  if(v > 0){
    px(x0,   y-6, COL_GREEN);
    px(x0+1, y-6, COL_GREEN);
  } else if(v < 0){
    px(x0,   y-4, COL_GREEN);
    px(x0+1, y-4, COL_GREEN);
  }

  drawNumber(x0+4, y-7, abs(v), COL_GREEN);
}

/* ================== SPEED ================== */
static int spdF = 0;

void renderAirspeedTape(int spd){
  const int TW = 18;
  const int x0 = 0;
  const int y0 = TOP_TAPE_H;
  const int h  = 96;

  spd = constrain(spd, 0, 300);
  spdF += (spd - spdF) >> 3;

  fillRect(x0+1, y0+1, TW-2, h-2, COL_BLACK);

  int R = h - 4;

  for(int v=0; v<=300; v+=10){
    int yy = y0 + h - 2 - (v * R) / 300;
    int L  = (v % 100 == 0) ? 6 : (v % 50 == 0) ? 4 : 2;
    for(int i=0;i<L;i++)
      px(x0+1+i, yy, COL_WHITE);
  }

  int yG = y0 + h - 2 - (80  * R) / 300;
  int yY = y0 + h - 2 - (160 * R) / 300;

  fillRect(x0+5, yG, TW-10, (y0+h-2)-yG, COL_GREEN);
  fillRect(x0+5, yY, TW-10, yG-yY,        COL_YELLOW);
  fillRect(x0+5, y0+2, TW-10, yY-(y0+2),  COL_MAGENTA);

  int yM = y0 + h - 2 - (spdF * R) / 300;
  fillRect(x0, yM-1, 4, 3, COL_WHITE);
  px(x0+5, yM, COL_WHITE);
}

void drawSpeedWindow(int spd){
  const int winW = 30;
  const int winH = 16;
  const int x0   = 2;
  const int y0   = CY - winH/2;

  spd = constrain(spd, 0, 999);

  fillRect(x0, y0, winW, winH, COL_BLACK);

  for(int x=x0; x<x0+winW; x++){
    px(x, y0,        COL_GREEN);
    px(x, y0+winH-1, COL_GREEN);
  }
  for(int y=y0; y<y0+winH; y++){
    px(x0,        y, COL_GREEN);
    px(x0+winW-1, y, COL_GREEN);
  }

  drawNumber(x0+6, y0+5, spd, COL_GREEN);
}

/* ================== ALTITUDE ================== */
static int altF = 0;

void renderAltitudeTape(int alt){
  const int TW = 18;
  const int x0 = W - TW;
  const int y0 = TOP_TAPE_H;
  const int h  = 96;

  alt = constrain(alt, 0, 20000);
  altF += (alt - altF) >> 3;

  fillRect(x0+1, y0+1, TW-2, h-2, COL_BLACK);

  int R = h - 4;

  for(int v=0; v<=20000; v+=500){
    int yy = y0 + h - 2 - (v * R) / 20000;
    int L  = (v % 5000 == 0) ? 6 : (v % 2500 == 0) ? 4 : 2;
    for(int i=0;i<L;i++)
      px(x0+TW-1-i, yy, COL_WHITE);
  }

  int yLow = y0 + h - 2 - (3000  * R) / 20000;
  int yMed = y0 + h - 2 - (10000 * R) / 20000;

  fillRect(x0+5, yLow, TW-10, (y0+h-2)-yLow, COL_GREEN);
  fillRect(x0+5, yMed, TW-10, yLow-yMed,     COL_YELLOW);
  fillRect(x0+5, y0+2, TW-10, yMed-(y0+2),   COL_MAGENTA);

  int yM = y0 + h - 2 - (altF * R) / 20000;
  fillRect(x0+TW-4, yM-1, 4, 3, COL_WHITE);
  px(x0+TW-6, yM, COL_WHITE);
}

void drawAltitudeWindow(int alt){
  const int winW = 30;
  const int winH = 16;
  const int x0   = W - winW - 2;
  const int y0   = CY - winH/2;

  int altDisp = (alt + 50) / 100;

  fillRect(x0, y0, winW, winH, COL_BLACK);

  for(int x=x0; x<x0+winW; x++){
    px(x, y0,         COL_GREEN);
    px(x, y0+winH-1,  COL_GREEN);
  }
  for(int y=y0; y<y0+winH; y++){
    px(x0,         y, COL_GREEN);
    px(x0+winW-1,  y, COL_GREEN);
  }

  drawNumber(x0+6, y0+5, altDisp % 1000, COL_GREEN);
}

/* ================== NAV ================== */
void drawCDIScale(){
  const int spacing = 5;
  const int y = CY + 18;

  for(int i=-2;i<=2;i++){
    int x = CX + i * spacing;
    px(x,   y,   COL_WHITE);
    px(x+1, y,   COL_WHITE);
    px(x,   y+1, COL_WHITE);
    px(x+1, y+1, COL_WHITE);
  }
}

void drawCDI(int course,int bearing){
  float dev = sin((bearing - course) * DEG_TO_RAD);
  dev = constrain(dev, -1.0f, 1.0f);

  int dx = dev * CDI_PIXELS;
  int x  = CX + dx;
  int y0 = CY + 14;

  for(int y=0;y<24;y++){
    px(x,   y0+y, COL_GREEN);
    px(x+1, y0+y, COL_GREEN);
  }
}

void drawToFromText(int course,int bearing){
  int diff = ((bearing - course + 540) % 360) - 180;
  bool toFlag = abs(diff) <= 90;
  int y = CY + 34;

  if(toFlag){
    for(int i=0;i<3;i++) px(CX-3+i, y, COL_GREEN);
    for(int i=0;i<5;i++) px(CX-2, y+i, COL_GREEN);
    px(CX+1, y, COL_GREEN);
    px(CX+3, y, COL_GREEN);
  }else{
    for(int i=0;i<5;i++) px(CX-3, y+i, COL_GREEN);
    px(CX-2, y, COL_GREEN);
    px(CX-1, y, COL_GREEN);
    for(int i=0;i<5;i++) px(CX+2, y+i, COL_GREEN);
  }
}

/* ================== POINTERS ================== */
void drawCoursePointer(int heading,int course){
  float a = (course - heading) * DEG_TO_RAD;

  for(int r=10;r<46;r++){
    px(CX + sinf(a)*r, CY - cosf(a)*r, COL_MAGENTA);
    px(CX - sinf(a)*r, CY + cosf(a)*r, COL_MAGENTA);
  }
}

void drawBearingPointer(int heading,int bearing){
  float a = (bearing - heading) * DEG_TO_RAD;

  for(int r=10;r<46;r++)
    px(CX + sinf(a)*r, CY - cosf(a)*r, COL_MAGENTA);
}

/* ================== HEADING ================== */
void drawHeadingTape(int heading,int bug){
  fillRect(0, 0, W, TOP_TAPE_H, COL_BLACK);

  int base = (heading / 5) * 5;

  for(int d=-60; d<=60; d+=5){
    int hdg = (base + d + 360) % 360;
    int x   = CX + d;
    if((unsigned)x >= W) continue;

    int len = (hdg % 10 == 0) ? 6 : 3;
    for(int i=0;i<len;i++)
      px(x, TOP_TAPE_H-1-i, COL_WHITE);

    if(hdg % 30 == 0)
      drawNumber(x-4, 1, hdg/10, COL_WHITE);
  }

  int diff = ((bug - heading + 540) % 360) - 180;
  if(abs(diff) <= 60){
    int bx = CX + diff;
    px(bx,   2, COL_MAGENTA);
    px(bx-1, 3, COL_MAGENTA);
    px(bx+1, 3, COL_MAGENTA);
  }

  for(int i=0;i<8;i++)
    px(CX, TOP_TAPE_H-1-i, COL_GREEN);
}

void drawHeadingWindow(int heading){
  int x0 = CX - 19;
  int y0 = 1;

  fillRect(x0, y0, 38, 14, COL_BLACK);

  for(int x=x0;x<x0+38;x++){
    px(x, y0,     COL_GREEN);
    px(x, y0+13,  COL_GREEN);
  }
  for(int y=y0;y<y0+14;y++){
    px(x0,    y, COL_GREEN);
    px(x0+37, y, COL_GREEN);
  }

  drawNumber(x0+6, y0+5, heading, COL_GREEN);
}
