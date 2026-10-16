/**************************************************************
 * MAP MODULE â€“ IMPLEMENTATION
 *
 * Moving Map fÃ¼r 128x128 Displays
 * ------------------------------------------------------------
 * Funktionen:
 *  - Hintergrundkarte zeichnen (Bitmap)
 *  - Track-History (Breadcrumbs)
 *  - Eigenes Flugzeugsymbol
 *  - Heading-abhÃ¤ngige Rotation (Track-Up)
 *
 * AbhÃ¤ngigkeiten:
 *  - pfd.h   â†’ Framebuffer-Zugriff (px, backFB, Farben)
 *  - map.h   â†’ Ã–ffentliche Schnittstelle
 *  - map_128x128_final.h â†’ PROGMEM Bitmap
 *************************************************************/

#include "map.h"
#include "pfd.h"
#include "map_128x128_final.h"
#include <math.h>

/* ==========================================================
 * MAP STATE (globale ZustÃ¤nde der Karte)
 * ========================================================== */

/* Eigene Position (Demo / spÃ¤ter GPS) */
float ownLat = 48.1234f;
float ownLon = 16.9876f;

/* Track-History (Breadcrumbs) */
float trackLat[TRACK_LEN];
float trackLon[TRACK_LEN];
uint8_t trackHead   = 0;
bool trackFilled    = false;


/* ==========================================================
 * MAP BACKGROUND
 * ----------------------------------------------------------
 * Kopiert die vorberechnete Bitmap aus dem Flash
 * direkt in den Framebuffer.
 * ========================================================== */
void drawMapBackground()
{
  for(int i = 0; i < W * H; i++){
    backFB[i] = pgm_read_word(&mapBitmap[i]);
  }
}


/* ==========================================================
 * MAP TRACK (Breadcrumbs)
 * ----------------------------------------------------------
 * Zeichnet die Flugspur relativ zur aktuellen Position.
 *
 * Schritte:
 *  1. Differenz Lat/Lon â†’ Meter
 *  2. Drehung in Flugzeugrichtung (Heading)
 *  3. Skalierung â†’ Pixel
 * ========================================================== */
void drawMapTrack(float lat, float lon, int heading)
{
  const float metersPerPixel = 6.0f;      // Zoomfaktor
  float hdg = heading * DEG_TO_RAD;

  int count = trackFilled ? TRACK_LEN : trackHead;

  for(int i = 0; i < count; i++){

    /* --- Differenz in geografischen Koordinaten --- */
    float dLat = trackLat[i] - lat;
    float dLon = trackLon[i] - lon;

    /* --- Umrechnung in Meter --- */
    float dy = dLat * 110540.0f;
    float dx = dLon * 111320.0f * cos(lat * DEG_TO_RAD);

    /* --- Drehung in Heading-Richtung (Track-Up) --- */
    float rx =  dx * cos(hdg) + dy * sin(hdg);
    float ry = -dx * sin(hdg) + dy * cos(hdg);

    /* --- Skalierung â†’ Display-Koordinaten --- */
    int x = CX + rx / metersPerPixel;
    int y = CY - ry / metersPerPixel;

    /* --- Punkt zeichnen --- */
    if(x > 1 && x < W - 2 && y > 1 && y < H - 2){
      px(x, y, COL_GREEN);
    }
  }
}


/* ==========================================================
 * MAP HEADING ARROW
 * ----------------------------------------------------------
 * Zeichnet einen einfachen Pfeil in Heading-Richtung
 * (Eigenbewegung).
 * ========================================================== */
void drawMapHeadingArrow(int heading)
{
  float a = heading * DEG_TO_RAD;

  for(int i = 0; i < 30; i++){
    int x = CX + sin(a) * i;
    int y = CY - cos(a) * i;

    if(x > 0 && x < W && y > 0 && y < H){
      px(x, y, COL_MAGENTA);
    }
  }
}

/* ==========================================================
 * MAP AIRCRAFT SYMBOL â€“ DOUBLE CHEVRON
 * ----------------------------------------------------------
 * - echtes Flugzeugsymbol (V-Form)
 * - doppellinig (besser sichtbar)
 * - grÃ¶ÃŸer, fÃ¼r 128x128 optimiert
 * - Heading-gedreht (Track-Up)
 *
 * Darstellung (schematisch):
 *
 *      \    |    /
 *       \   |   /
 *        \  |  /
 * ========================================================== */
void drawMapAircraftChevron(int heading)
{
  const int sizeOuter = 25;      // Ã¤uÃŸere Chevron-Linie
  const int sizeInner = 20;      // innere Chevron-Linie
  const int wingAngle = 150;    // Ã–ffnungswinkel (Grad)

  float a = heading * DEG_TO_RAD;

  float aL = a + wingAngle * DEG_TO_RAD;
  float aR = a - wingAngle * DEG_TO_RAD;

  int cx = CX;
  int cy = CY;

  /* ---------- Ã¤uÃŸeres Chevron ---------- */
  for(int i = 0; i <= sizeOuter; i++){
    int xl = cx + sin(aL) * i;
    int yl = cy - cos(aL) * i;
    int xr = cx + sin(aR) * i;
    int yr = cy - cos(aR) * i;

    if(xl > 0 && xl < W && yl > 0 && yl < H) px(xl, yl, COL_YELLOW);
    if(xr > 0 && xr < W && yr > 0 && yr < H) px(xr, yr, COL_YELLOW);
  }

  /* ---------- inneres Chevron ---------- */
  for(int i = 0; i <= sizeInner; i++){
    int xl = cx + sin(aL) * i;
    int yl = cy - cos(aL) * i;
    int xr = cx + sin(aR) * i;
    int yr = cy - cos(aR) * i;

    if(xl > 0 && xl < W && yl > 0 && yl < H) px(xl, yl, COL_YELLOW);
    if(xr > 0 && xr < W && yr > 0 && yr < H) px(xr, yr, COL_YELLOW);
  }

  /* ---------- Nase (doppellinig) ---------- */
  for(int i = 0; i < 6; i++){
    int xn = cx + sin(a) * i;
    int yn = cy - cos(a) * i;

    if(xn > 0 && xn < W && yn > 0 && yn < H) px(xn, yn, COL_YELLOW);
    if(xn+1 > 0 && xn+1 < W && yn > 0 && yn < H) px(xn+1, yn, COL_YELLOW);
  }
}
