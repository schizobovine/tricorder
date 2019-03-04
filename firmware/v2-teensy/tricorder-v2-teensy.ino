/*
 * tricorder-v2-teensy.ino
 *
 * Teensy 3.2 based sensor thingy. Version 2: better, strong, now with more
 * calcium!
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
 * Hardware Revsion: Mk 1.7 Featherwing with Adafruit Teensy adapter
 *
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <ssd1351.h>
#include <Adafruit_MLX90614.h>
#include <ClosedCube_HDC1080.h>
#include <VEML6075.h>
#include <Time.h>
#include <LTC2941.h>
#include <SFE_LSM9DS0.h>
#include <MS5611.h>
#include <TCS3400.h>
//#include <Adafruit_VEML6075.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Pin config

// Ugh. To use the optimized ssd1351 code, the DC and CS pins need to be
// hardware CS pins on separate masks. Thus my use of the analog pins here
// won't fly. Damnit.

// Alternate options, from reading SPI.pinIsChipSelect(uint8_t, uint8_t):

// mask cs1 cs2 cs3
// 0x01 10  2   26
// 0x02 9   6
// 0x04 20  23
// 0x08 21  22
// 0x10 15
// 0x20 45

#define DISP_DC      (15)
#define DISP_RST     (14)
#define DISP_CS      (10)
#define SD_CS        (16)
#define SD_CARDSW    (17)
#define BATT_DIV     (21)

// 16-bit color is weird: 5 bits for R & B, but 6 for G? I guess humans are
// more sensitive to it...

// How many bits of color ya want?
//typedef ssd1351::HighColor Color;    // 18 (6/chan)
typedef ssd1351::LowColor Color;     // 16 (5/6/5)
//typedef ssd1351::IndexedColor Color; // 8

// Enable or disable double buffer
//typedef ssd1351::NoBuffer Buffer;
typedef ssd1351::SingleBuffer Buffer;

// Display dimensions
#define DISP_H (128)
#define DISP_W (128)

// Define some color consts

#define COLOR_RED   (0xF800) // 0b1111100000000000 (5)
#define COLOR_GREEN (0x07E0) // 0b0000011111100000 (6)
#define COLOR_BLUE  (0x001F) // 0b0000000000011111 (5)

// Composite colors

#define COLOR_BLACK    (0x0000) // 0b0000000000000000 (  0%,   0%,   0%)
#define COLOR_BROWN    (0xA145) // 0b1010000101000101 ( 64%,  16%,  16%)
#define COLOR_MAGENTA  (0xF81F) // 0b1111100000011111 (100%,   0%, 100%)
#define COLOR_ORANGE   (0xFD20) // 0b1111110100100000 (100%,  64%,   0%)
#define COLOR_YELLOW   (0xFFE0) // 0b1111111111100000 (100%, 100%,   0%)
#define COLOR_CYAN     (0x07FF) // 0b0000011111111111 (  0%, 100%, 100%)
#define COLOR_PURPLE   (0xA11E) // 0b1010000100011110 ( 62%,  12%,  94%)
#define COLOR_LAVENDER (0XE73F) // 0b1110011100111111 ( 90%,  90%,  98%)
#define COLOR_WHITE    (0xFFFF) // 0b1111111111111111 (100%, 100%, 100%)

// "Special" characters
//#define STR_DEGREE  "\367"
//#define STR_SQUARED "\374"
//#define STR_DEG "\371" // 249 (substituted degress centigrade)
//#define PCT "\372" // 250 (substituted percent)
#define STR_DEG   "\216"
#define STR_DEG_C "\216C"

// Misc
#define SD_FILENAME ("tricorder.log")
#define ANALOG_RES (10)
#define I2C_ADDR_HDC1080    0x40
#define I2C_ADDR_LSM9DS0_XM 0x1D //0x1E if SDO_XM is LOW
#define I2C_ADDR_LSM9DS0_G  0x6B //0x6A if SDO_G is LOW

// MicroSD card log file
File logfile;
bool card_present = false;

// Display controller
auto display = ssd1351::SSD1351<Color,Buffer,DISP_H,DISP_W>(DISP_CS, DISP_DC, DISP_RST);

// Sensors
VEML6075           veml6075  = VEML6075();
//Adafruit_VEML6075  veml6075  = Adafruit_VEML6075();
Adafruit_MLX90614  mlx90614  = Adafruit_MLX90614();
ClosedCube_HDC1080 hdc1080   = ClosedCube_HDC1080();
LSM9DS0            lsm9ds0   = LSM9DS0(MODE_I2C, I2C_ADDR_LSM9DS0_G, I2C_ADDR_LSM9DS0_XM);
MS5611             ms5611    = MS5611();
TCS3400            tcs3400   = TCS3400();

// Sensor values, current and last different value. The later is used to forego
// screen updates (which are unfortunately slow) if the value has not changed.

typedef struct sensval {
  float uva = -1.0;           // (veml6075) UV index
  float uvb = -1.0;           // (veml6075) UVA channel
  float uvi = -1.0;           // (veml6075) UVB channel
  uint16_t raw_dark = 0;      // (veml6075) raw IR channel
  uint16_t raw_vis_comp = 0;  // (veml6075) raw vis channel
  uint16_t raw_ir_comp = 0;   // (veml6075) raw dark channel
  float temp = -1.0;          // (hdc1080) temp (ambient)
  float rh = -1.0;            // (hdc1080) relative humidity
  float color_temp = -1.0;    // (tcs3400) color temp
  float batt_v = -1.0;        // (ltc2942) XXX need to get that chip in there
  float batt_ma = -1.0;       // (ltc2942) TODO
  float pressure = -1.0;      // (ms5611) pressure (absolute)
  float lat = -1.0;           // (gps) latitude
  float lon = -1.0;           // (gps) longitude
  float alt = -1.0;           // (ms5611) altitude (m)
  float irtemp = -1.0;        // (mlx90614) temp (remote)
  uint16_t color_r = 0xFFFF;  // (tcs3400) red
  uint16_t color_g = 0xFFFF;  // (tcs3400) green
  uint16_t color_b = 0xFFFF;  // (tcs3400) blue
  uint16_t vis = 0x0;         // (tcs3400) clear (visible)
  uint16_t ir = 0x0;          // (tcs3400) clear (visible)
  float acc_x = 0;            // (lsm9ds0) accelerometer
  float acc_y = 0;
  float acc_z = 0;
  float mag_x = 0;            // (lsm9ds0) magnetometer
  float mag_y = 0;
  float mag_z = 0;
  float gyr_x = 0;            // (lsm9ds0) gyroscope
  float gyr_y = 0;
  float gyr_z = 0;

} sensval_t;

sensval_t curr, last;

//
// Helper macros
//

#define USEC_DIFF(x, y) \
  (((x) > (y)) ? ((x) - (y)) : ((x) + (ULONG_MAX - (y))))

#define DISPLAY_LABEL(x, y, val) \
  do { \
    display.setCursor(x, y); \
    display.print(val); \
  } while (0)

#define DISPLAY_LABEL_RGB(x, y, val, color) \
  do { \
    display.setCursor(x, y); \
    display.setTextColor(color); \
    display.print(val); \
    display.setTextColor(COLOR_WHITE); \
  } while (0)

#define DISPLAY_READING(x, y, print_arg, name) \
  do { \
    /*if (last.name != curr.name) { */ \
      last.name = curr.name; \
      display.setCursor((x), (y)); \
      display.print(curr.name, print_arg); \
    /*}*/ \
  } while (0);

#define DISPLAY_READING_UNIT(x, y, print_arg, name, unit) \
  do { \
    /*if (last.name != curr.name) { */ \
      last.name = curr.name; \
      display.setCursor((x), (y)); \
      display.print(curr.name, print_arg); \
      display.print(unit); \
    /*}*/ \
  } while (0);

#define C_TO_F(x) ((x) * 9.0/5.0 + 32.0)

////////////////////////////////////////////////////////////////////////
// HALPING
////////////////////////////////////////////////////////////////////////

float getBattVoltage() {
  //float measured = analogRead(BATT_DIV);
  //return (measured / ((float)(1 << ANALOG_RES))) * 2.0 * 3.3;
  return 0.0;
}

void displayLabels() {

  //            <x>, <y>, <label_text>
  DISPLAY_LABEL(  0,  16, F("RH"));                   // (hdc1080) relative humidity (%)
  DISPLAY_LABEL(  0,  24, F("Ta"));                   // (hdc1080) temp (ambient deg C)
  DISPLAY_LABEL(  0,  32, F("Tr"));                   // (mlx90614) temp (remote deg C)
  DISPLAY_LABEL(  0,  40, F("P"));                    // (ms5611) pressure (abs kPa)
  DISPLAY_LABEL(  0,  48, F("alt"));                  // (ms5611) altitude (m)
  //DISPLAY_LABEL( 60,  40, F("CT"));                   // (tcs3400) color temp (K)

  //                <x>, <y>, <text>, <color>
  DISPLAY_LABEL_RGB( 60,  16, F("R"), COLOR_RED);     // (tcs3400) red (word)
  DISPLAY_LABEL_RGB( 60,  24, F("G"), COLOR_GREEN);   // (tcs3400) green (word)
  DISPLAY_LABEL_RGB( 60,  32, F("B"), COLOR_BLUE);    // (tcs3400) blue (word)
                 
  //            <x>, <y>, <label_text>
  DISPLAY_LABEL( 60,  40, F("lat"));                  // (gps) latitude (deg)
  DISPLAY_LABEL( 60,  48, F("lon"));                  // (gps) longitude (deg)
  DISPLAY_LABEL(  0,  56, F("IR"));                   // (tcs3400) IR
  DISPLAY_LABEL(  0,  64, F("VIS"));                  // (tcs3400) visible
  DISPLAY_LABEL(  0,  72, F("UVI"));                  // (veml6075) UV index
  DISPLAY_LABEL(  0,  80, F("UVA"));                  // (veml6075) UVA channel
  DISPLAY_LABEL(  0,  88, F("UVB"));                  // (veml6075) UVB channel
  DISPLAY_LABEL( 70,  72, F("rIR"));                  // (veml6075) raw IR channel
  DISPLAY_LABEL( 66,  80, F("rVIS"));                 // (veml6075) raw vis channel
  DISPLAY_LABEL( 66,  88, F("dark"));                 // (veml6075) raw dark channel
  DISPLAY_LABEL(  0, 104, F("Acc (g)"));              // (lsm9ds0) accelerometer (g)
  DISPLAY_LABEL(  0, 112, F("Mag (G)"));              // (lsm9ds0) magnetometer (gauss)
  DISPLAY_LABEL(  0, 120, F("Gyr (" STR_DEG "/s)"));  // (lsm9ds0) gyroscope (deg/s)
  DISPLAY_LABEL( 36,  96, F("X"));                    // (lsm9ds0) x-values
  DISPLAY_LABEL( 72,  96, F("Y"));                    // (lsm9ds0) y-values
  DISPLAY_LABEL(108,  96, F("Z"));                    // (lsm9ds0) z-values

}

void displayValues_tcs3400() {
  // x, y, print_arg, $name
  DISPLAY_READING( 74,  16, DEC, color_r);
  DISPLAY_READING( 74,  24, DEC, color_g);
  DISPLAY_READING( 74,  32, DEC, color_b);
  DISPLAY_READING( 14,  56, DEC, ir);
  DISPLAY_READING( 14,  64, DEC, vis);
  //DISPLAY_READING_UNIT( 70,  40, 0, color_temp, "K");
}

void displayValues_veml6075() {
  // x, y, print_arg, $name
  DISPLAY_READING( 14, 72, 2, uvi);
  DISPLAY_READING( 14, 80, 2, uva);
  DISPLAY_READING( 14, 88, 2, uvb);
  DISPLAY_READING( 84, 72, DEC, raw_ir_comp);
  DISPLAY_READING( 84, 80, DEC, raw_vis_comp);
  DISPLAY_READING( 84, 88, DEC, raw_dark);
}

void displayValues_mlx90614() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT(14, 32, 1, irtemp, STR_DEG_C);
}

void displayValues_hdc1080() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT(14, 16, 0, rh, "%");
  DISPLAY_READING_UNIT(14, 24, 1, temp, STR_DEG_C);
}

void displayValues_ms5611() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT( 14,  40, 2, pressure, " kPa");
  DISPLAY_READING_UNIT( 14,  48, 0,       alt, " m");
}

void displayValues_lsm9ds0() {
  // x, y, print_arg, $name
  DISPLAY_READING( 36, 104, 2, acc_x);
  DISPLAY_READING( 72, 104, 2, acc_y);
  DISPLAY_READING(108, 104, 2, acc_z);
  DISPLAY_READING( 36, 112, 2, mag_x);
  DISPLAY_READING( 72, 112, 2, mag_y);
  DISPLAY_READING(108, 112, 2, mag_z);
  DISPLAY_READING( 36, 120, 2, gyr_x);
  DISPLAY_READING( 72, 120, 2, gyr_y);
  DISPLAY_READING(108, 120, 2, gyr_z);
}

void displayValues_gps() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT( 74,  40, 3, lat, STR_DEG);
  DISPLAY_READING_UNIT( 74,  48, 3, lon, STR_DEG);
}

void displayValues_batt() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT( 106,   8, 2, batt_v, "V");
}

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {

  delay(500);
  Serial.begin(115200);
  Serial.println(F("Tricorder v2.0-teensy (Built " __DATE__ " " __TIME__")"));

  // Setup ADC
  analogReadResolution(ANALOG_RES);

  // Check if SD card is present (shorts to ground if not present)
  pinMode(SD_CARDSW, INPUT_PULLUP);
  if (digitalRead(SD_CARDSW) == HIGH) {
    if (SD.begin(SD_CS)) {
      logfile = SD.open(SD_FILENAME, FILE_WRITE);
      if (logfile) {
        card_present = true;
      }
    }
  }
  Serial.println("SD init complete");

  // Init display
  delay(100);
  display.begin();
  display.sleep(false);
  display.fillScreen(COLOR_BLACK);
  display.setTextColor(COLOR_WHITE, COLOR_BLACK);
  display.setTextSize(1);
  display.setTextWrap(false);
  displayLabels();
  display.updateScreen();
  Serial.println("display init complete");

  // Init i2C
  Wire.begin();

  // Init sensors
  veml6075.begin();
  mlx90614.begin(); 
  hdc1080.begin(I2C_ADDR_HDC1080);
  uint16_t status = lsm9ds0.begin();
  if (status != 0x49D4) {
    Serial.print(F("LSM9DS0 init failed! status="));
    Serial.println(status, HEX);
  }
  if (!ms5611.begin()) {
    Serial.print(F("MS5611 not found!"));
  }
  if (!tcs3400.begin()) {
    Serial.print(F("TCS3400 not found!"));
  }
  Serial.println("i2c init complete");

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {

  // Poll RTC for time (UTC)
  //static DateTime ts;
  //static String now;
  //ts = rtc.now();
  //ts.iso8601(now);

  // Get battery status
  //curr.batt_v = getBattVoltage();
  //curr.batt_ma = 0.0;
  
  // Poll VEML6075
  //curr.uva = veml6075.readUVA();
  //curr.uvb = veml6075.readUVB();
  //curr.uvi = veml6075.readUVI();
  veml6075.poll();
  curr.uva = veml6075.getUVA();
  curr.uvb = veml6075.getUVB();
  curr.uvi = veml6075.getUVIndex();
  curr.raw_dark = veml6075.getRawDark();
  curr.raw_ir_comp = veml6075.getRawIRComp();
  curr.raw_vis_comp = veml6075.getRawVisComp();

  // Poll TCS3400
  curr.ir = tcs3400.getIR();
  curr.vis = tcs3400.getVisible();
  curr.color_r = tcs3400.getRed();
  curr.color_g = tcs3400.getGreen();
  curr.color_b = tcs3400.getBlue();

  // Poll MS5611
  curr.pressure = ms5611.readPressure();
  curr.alt = ms5611.getAltitude(curr.pressure);
  curr.pressure /= 1000.0; //convert to kPa for display

  // Poll HDC1080
  curr.temp = hdc1080.readTemperature();
  curr.rh = hdc1080.readHumidity();

  // Poll MLX90614
  curr.irtemp = mlx90614.readObjectTempC();

  // Poll LSM9DS0 accelerometer
  lsm9ds0.readAccel();
  curr.acc_x = lsm9ds0.calcAccel(lsm9ds0.ax); // converts to g
  curr.acc_y = lsm9ds0.calcAccel(lsm9ds0.ay); // (i.e., 1g = 9.8m/s^2)
  curr.acc_z = lsm9ds0.calcAccel(lsm9ds0.az);

  // Poll LSM9DS0 magnetometer
  lsm9ds0.readMag();
  curr.mag_x = lsm9ds0.calcMag(lsm9ds0.mx); // converts to Gauss
  curr.mag_y = lsm9ds0.calcMag(lsm9ds0.my);
  curr.mag_z = lsm9ds0.calcMag(lsm9ds0.mz);

  // Poll LSM9DS0 gyroscope
  lsm9ds0.readGyro();
  curr.gyr_x = lsm9ds0.calcGyro(lsm9ds0.gx); // converts to deg/s
  curr.gyr_y = lsm9ds0.calcGyro(lsm9ds0.gy);
  curr.gyr_z = lsm9ds0.calcGyro(lsm9ds0.gz);


  // Display readings refresh
  display.fillScreen(COLOR_BLACK);
  displayLabels();
  displayValues_hdc1080(); 
  displayValues_mlx90614();
  displayValues_veml6075();
  displayValues_tcs3400();
  displayValues_ms5611();
  displayValues_lsm9ds0();
  displayValues_gps();
  displayValues_batt();
  display.updateScreen();

  // Delay between data polls
  delay(500);

}
