// map.h
#pragma once
#include <Arduino.h>

/* ================== MAP STATE ================== */
#define TRACK_LEN 64

extern float trackLat[TRACK_LEN];
extern float trackLon[TRACK_LEN];
extern uint8_t trackHead;
extern bool trackFilled;

extern float ownLat;
extern float ownLon;

/* ================== MAP DRAW ================== */
void drawMapBackground();
void drawMapTrack(float lat, float lon, int heading);
void drawMapHeadingArrow(int heading);
void drawMapAircraftChevron(int heading);
