# RC-Cockpit

RC flight-instrument cockpit display for model aircraft, running on a Waveshare RP2350 (Core2350B).

## Features

- Two ST7735 128×128 TFT displays, driven independently via SPI:
  - **Display 1 (left):** Primary Flight Display — artificial horizon, pitch ladder, airspeed/altitude tapes, VSI, heading tape, CDI/course/bearing pointers
  - **Display 2 (right):** Moving map with track history and aircraft symbol, track-up orientation
- IMU-driven attitude and heading (ICM-20948, complementary filter for pitch/roll, tilt-compensated magnetometer heading)
- Servo-driven pilot head with randomized, roll-biased "gaze" behavior (non-blocking state machine, eased motion)
- Three-position switch input (toggle logic, not level-based): up-shift toggles pilot head motion, down-shift toggles both displays
- Non-blocking Garmin-style boot screen, shown once on first display activation, not on every boot
- Safe startup state: displays dark and pilot head motionless until actively switched on

## Hardware

Waveshare Core2350B (RP2350), two ST7735 128×128 SPI displays, one analog servo (pilot head), SparkFun ICM-20948 9-DoF IMU breakout.

| Function | GPIO |
|---|---|
| SPI SCK | 6 |
| SPI MOSI | 7 |
| Display 1 CS | 8 |
| Display 2 CS | 9 |
| Display DC | 12 |
| Display RST | 13 |
| Display 1 backlight | 14 |
| Display 2 backlight | 15 |
| IMU SDA | 4 |
| IMU SCL | 5 |
| Pilot servo | 21 |
| 3-position switch (PWM in) | 10 |

GPIO 0/1 are intentionally left free for a planned SRXL2 connection.

## Dependencies (Arduino IDE / arduino-pico core)

- [arduino-pico](https://github.com/earlephilhower/arduino-pico) board support for RP2040/RP2350
- [SparkFun ICM-20948 Arduino Library](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary)

## Status

Feature-frozen as an Arduino sketch. The PWM switch input is being replaced by SRXL2 channel decoding as part of a combined firmware effort with [MSRC](https://github.com/rgnovak/msrc) (`msrc-cockpit` branch) — porting this display/servo logic from Arduino to the raw Pico SDK so it can run alongside MSRC's telemetry code on one chip.