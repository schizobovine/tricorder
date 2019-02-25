/*
 * spectrometer.ino
 *
 * Cheap spectrometer based on Sparkfun's AS7265x triple breakout board, using
 * a Teensy 3.2 as the brain and Adafruit's 1.8" TFT display.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
 *
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>

#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>

#include <SparkFun_AS7265X.h>

// Pin config
#define DISP_DC      (15)
#define DISP_RST     (14)
#define DISP_CS      (10)
#define DISP_LITE    (17)
#define SD_CS        (16)
#define BATT_DIV     (21)

// Display dimensions
#define DISP_H (160)
#define DISP_W (128)

// Backlight brightness (0-255)
#define DISP_PWM (191)

// Other display constants / magic numbers
#define DISP_LABEL_W      (42)
#define DISP_LINE_H       (9)
#define DISP_READING_W    (DISP_W - DISP_LABEL_W)
#define DISP_READING_X    (DISP_LABEL_W + 5)

// Misc defines
#define SD_FILENAME ("debug.log")
#define ANALOG_RES (10)

// MicroSD card log file
File logfile;
bool card_present = false;

// Display controller
Adafruit_ST7735 display = Adafruit_ST7735(DISP_CS, DISP_DC, DISP_RST);

// Sensors
AS7265X spectrometer;

// Sensor values, current and last different value. The later is used to forego
// screen updates (which are unfortunately slow) if the value has not changed.

typedef struct sensvalcal {
  float a; // A 410nm
  float b; // B 435nm
  float c; // C 460nm
  float d; // D 485nm
  float e; // E 510nm
  float f; // F 535nm
  float g; // G 560nm
  float h; // H 585nm
  float r; // R 610nm
  float i; // I 645nm
  float s; // S 680nm
  float j; // J 705nm
  float t; // T 730nm
  float u; // U 760nm
  float v; // V 810nm
  float w; // W 860nm
  float k; // K 900nm
  float l; // L 940nm
} sensval_cal_t;

typedef struct sensvalraw {
  uint16_t a; // A 410nm
  uint16_t b; // B 435nm
  uint16_t c; // C 460nm
  uint16_t d; // D 485nm
  uint16_t e; // E 510nm
  uint16_t f; // F 535nm
  uint16_t g; // G 560nm
  uint16_t h; // H 585nm
  uint16_t r; // R 610nm
  uint16_t i; // I 645nm
  uint16_t s; // S 680nm
  uint16_t j; // J 705nm
  uint16_t t; // T 730nm
  uint16_t u; // U 760nm
  uint16_t v; // V 810nm
  uint16_t w; // W 860nm
  uint16_t k; // K 900nm
  uint16_t l; // L 940nm
} sensval_raw_t;

typedef struct sensval {
  sensval_raw_t raw;
  sensval_cal_t cal;
} sensval_t;

sensval_t curr, last;

//
// Helper macros
//

#define DEBUG 1

#if defined(DEBUG)
#define DEBUG_PRINT(...) \
  do { \
    if (Serial)       {  Serial.print(__VA_ARGS__); } \
    if (card_present) { logfile.print(__VA_ARGS__); } \
  } while (0)
#define DEBUG_PRINTLN(...) \
  do { \
    if (Serial)       {  Serial.println(__VA_ARGS__); } \
    if (card_present) { logfile.println(__VA_ARGS__); } \
  } while (0)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#endif

#define USEC_DIFF(x, y) \
  (((x) > (y)) ? ((x) - (y)) : ((x) + (ULONG_MAX - (y))))

#define DISPLAY_LABEL(x, y, val) \
  do { \
    /*display.writeFillRect((x), (y), DISP_LABEL_W, DISP_LINE_H, ST77XX_BLACK);*/ \
    display.setCursor((x), (y)); \
    display.print((val)); \
  } while (0)

#define DISPLAY_READING(x, y, print_arg, name) \
  do { \
    if (last.name != curr.name) { \
      last.name = curr.name; \
      display.fillRect((x), (y), DISP_READING_W, DISP_LINE_H, ST77XX_BLACK); \
      display.setCursor((x), (y)); \
      display.print(curr.name, (print_arg)); \
    } \
  } while (0);

////////////////////////////////////////////////////////////////////////
// HALPING
////////////////////////////////////////////////////////////////////////

void displayLabels() {
  DISPLAY_LABEL(0,  0 * DISP_LINE_H,  "A 410nm");
  DISPLAY_LABEL(0,  1 * DISP_LINE_H,  "B 435nm");
  DISPLAY_LABEL(0,  2 * DISP_LINE_H,  "C 460nm");
  DISPLAY_LABEL(0,  3 * DISP_LINE_H,  "D 485nm");
  DISPLAY_LABEL(0,  4 * DISP_LINE_H,  "E 510nm");
  DISPLAY_LABEL(0,  5 * DISP_LINE_H,  "F 535nm");
  DISPLAY_LABEL(0,  6 * DISP_LINE_H,  "G 560nm");
  DISPLAY_LABEL(0,  7 * DISP_LINE_H,  "H 585nm");
  DISPLAY_LABEL(0,  8 * DISP_LINE_H,  "R 610nm");
  DISPLAY_LABEL(0,  9 * DISP_LINE_H,  "I 645nm");
  DISPLAY_LABEL(0, 10 * DISP_LINE_H,  "S 680nm");
  DISPLAY_LABEL(0, 11 * DISP_LINE_H,  "J 705nm");
  DISPLAY_LABEL(0, 12 * DISP_LINE_H,  "T 730nm");
  DISPLAY_LABEL(0, 13 * DISP_LINE_H,  "U 760nm");
  DISPLAY_LABEL(0, 14 * DISP_LINE_H,  "V 810nm");
  DISPLAY_LABEL(0, 15 * DISP_LINE_H,  "W 860nm");
  DISPLAY_LABEL(0, 16 * DISP_LINE_H,  "K 900nm");
  DISPLAY_LABEL(0, 17 * DISP_LINE_H,  "L 940nm");
}

void displayReadings() {
  DISPLAY_READING(DISP_READING_X,  0 * DISP_LINE_H, 1, cal.a);
  DISPLAY_READING(DISP_READING_X,  1 * DISP_LINE_H, 1, cal.b);
  DISPLAY_READING(DISP_READING_X,  2 * DISP_LINE_H, 1, cal.c);
  DISPLAY_READING(DISP_READING_X,  3 * DISP_LINE_H, 1, cal.d);
  DISPLAY_READING(DISP_READING_X,  4 * DISP_LINE_H, 1, cal.e);
  DISPLAY_READING(DISP_READING_X,  5 * DISP_LINE_H, 1, cal.f);
  DISPLAY_READING(DISP_READING_X,  6 * DISP_LINE_H, 1, cal.g);
  DISPLAY_READING(DISP_READING_X,  7 * DISP_LINE_H, 1, cal.h);
  DISPLAY_READING(DISP_READING_X,  8 * DISP_LINE_H, 1, cal.r);
  DISPLAY_READING(DISP_READING_X,  9 * DISP_LINE_H, 1, cal.i);
  DISPLAY_READING(DISP_READING_X, 10 * DISP_LINE_H, 1, cal.s);
  DISPLAY_READING(DISP_READING_X, 11 * DISP_LINE_H, 1, cal.j);
  DISPLAY_READING(DISP_READING_X, 12 * DISP_LINE_H, 1, cal.t);
  DISPLAY_READING(DISP_READING_X, 13 * DISP_LINE_H, 1, cal.u);
  DISPLAY_READING(DISP_READING_X, 14 * DISP_LINE_H, 1, cal.v);
  DISPLAY_READING(DISP_READING_X, 15 * DISP_LINE_H, 1, cal.w);
  DISPLAY_READING(DISP_READING_X, 16 * DISP_LINE_H, 1, cal.k);
  DISPLAY_READING(DISP_READING_X, 17 * DISP_LINE_H, 1, cal.l);
}

void logRawReadings() {
  DEBUG_PRINT(F("\"raw\","));
  DEBUG_PRINT(curr.raw.a); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.b); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.c); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.d); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.e); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.f); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.g); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.h); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.r); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.i); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.s); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.j); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.t); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.u); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.v); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.w); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.k); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.raw.l); DEBUG_PRINT(F(","));
  DEBUG_PRINTLN();
}

void logCalReadings() {
  DEBUG_PRINT(F("\"calibrated\","));
  DEBUG_PRINT(curr.cal.a); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.b); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.c); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.d); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.e); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.f); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.g); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.h); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.r); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.i); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.s); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.j); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.t); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.u); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.v); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.w); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.k); DEBUG_PRINT(F(","));
  DEBUG_PRINT(curr.cal.l); DEBUG_PRINT(F(","));
  DEBUG_PRINTLN();
}

void logCalibrationData() {
  float data;
  DEBUG_PRINT(F("\"calibrationData\","));
  for (int i=0; i<18; i++) {
    data = spectrometer.getCalibrationData(i);
    DEBUG_PRINT(data); DEBUG_PRINT(F(","));
  }
  DEBUG_PRINTLN();
}

sensval_t *getReadings(sensval_t *readings) {

  spectrometer.takeMeasurements();

  readings->raw.a = spectrometer.getA();
  readings->raw.b = spectrometer.getB();
  readings->raw.c = spectrometer.getC();
  readings->raw.d = spectrometer.getD();
  readings->raw.e = spectrometer.getE();
  readings->raw.f = spectrometer.getF();
  readings->raw.g = spectrometer.getG();
  readings->raw.h = spectrometer.getH();
  readings->raw.r = spectrometer.getR();
  readings->raw.i = spectrometer.getI();
  readings->raw.s = spectrometer.getS();
  readings->raw.j = spectrometer.getJ();
  readings->raw.t = spectrometer.getT();
  readings->raw.u = spectrometer.getU();
  readings->raw.v = spectrometer.getV();
  readings->raw.w = spectrometer.getW();
  readings->raw.k = spectrometer.getK();
  readings->raw.l = spectrometer.getL();

  readings->cal.a = spectrometer.getCalibratedA();
  readings->cal.b = spectrometer.getCalibratedB();
  readings->cal.c = spectrometer.getCalibratedC();
  readings->cal.d = spectrometer.getCalibratedD();
  readings->cal.e = spectrometer.getCalibratedE();
  readings->cal.f = spectrometer.getCalibratedF();
  readings->cal.g = spectrometer.getCalibratedG();
  readings->cal.h = spectrometer.getCalibratedH();
  readings->cal.r = spectrometer.getCalibratedR();
  readings->cal.i = spectrometer.getCalibratedI();
  readings->cal.s = spectrometer.getCalibratedS();
  readings->cal.j = spectrometer.getCalibratedJ();
  readings->cal.t = spectrometer.getCalibratedT();
  readings->cal.u = spectrometer.getCalibratedU();
  readings->cal.v = spectrometer.getCalibratedV();
  readings->cal.w = spectrometer.getCalibratedW();
  readings->cal.k = spectrometer.getCalibratedK();
  readings->cal.l = spectrometer.getCalibratedL();

  return readings;
  
}

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {

  delay(300);

  // Debugging info dumped over serial
  if (Serial) {
    Serial.begin(115200);
    Serial.println(F("Spectrometer v1.0-teensy (Built " __TIMESTAMP__ ")"));
  }

  // Setup ADC
  //analogReadResolution(ANALOG_RES);
  //DEBUG_PRINTLN("SD init complete");

  // Try to find / mount SD card and open log file on there
  if (SD.begin(SD_CS)) {
    logfile = SD.open(SD_FILENAME, FILE_WRITE);
    if (logfile) {
      card_present = true;
      DEBUG_PRINTLN("SD init complete");
    } else {
      DEBUG_PRINTLN("SD failed to open logfile");
    }
  } else {
    DEBUG_PRINTLN("SD init failed");
  }

  // Enable display backlight
  pinMode(DISP_LITE, OUTPUT);
  analogWrite(DISP_LITE, DISP_PWM);

  // Init display
  delay(100);
  display.initR(INITR_BLACKTAB);
  display.setRotation(2);
  display.fillScreen(ST77XX_BLACK);
  display.setCursor(0, 0);
  display.setTextColor(ST77XX_WHITE);
  display.setTextWrap(false);
  displayLabels();
  DEBUG_PRINTLN("display init complete");

  // Init i2C & spectrometer
  Wire.begin();
  if (!spectrometer.begin()) {
    DEBUG_PRINTLN("spectrometer init failed");
    while (1) {
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
    }
  } else {
    DEBUG_PRINTLN("spectrometer init complete");
  }
  DEBUG_PRINTLN("i2c init complete");

  logCalibrationData();
  DEBUG_PRINTLN("calibration data dump complete");

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  getReadings(&curr);
  displayReadings();

  logRawReadings();
  logCalibrationData();
  logCalReadings();
  delay(5000);

  //display.fillScreen(ST77XX_BLACK);
  //displayLabels();
}

/*
A
B
C
D
E
F
G
H
I
J
K
L
R
S
T
U
V
W
*/
