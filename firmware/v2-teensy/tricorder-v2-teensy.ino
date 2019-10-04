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
#include <Adafruit_MLX90614.h>
#include <Adafruit_SGP30.h>
#include <ClosedCube_HDC1080.h>
#include <EEPROM.h>
#include <LTC2941.h>
#include <MS5611.h>
#include <SD.h>
#include <SFE_LSM9DS0.h>
#include <ssd1351.h>
#include <TCS3400.h>
#include <Time.h>
#include <TinyGPS.h>
#include <VEML6075.h>

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
#define STR_DEG   "\216"
#define STR_DEG_C "\216C"
#define SQUARED   "\220"

// EEPROM addresses to store SGP30 calibration data
#define SGP30_EEPROM_ADDR_TIME (0x06) /*uint31_t*/
#define SGP30_EEPROM_ADDR_ECO2 (0x10) /*uint16_t*/
#define SGP30_EEPROM_ADDR_TVOC (0x12) /*uint16_t*/

// Misc
#define SD_FILENAME ("tricorder.log")
#define ANALOG_RES (10)
#define I2C_ADDR_HDC1080    0x40
#define I2C_ADDR_LSM9DS0_XM 0x1D //0x1E if SDO_XM is LOW
#define I2C_ADDR_LSM9DS0_G  0x6B //0x6A if SDO_G is LOW
#define GPS_UART (Serial3)
#define GPS_UART_BAUD (9600)

// Physical constants (used for relative->abs humidity calc)
const float WATER_G_PER_MOLE = 18.01534; // g/mol
const float GAS_CONSTANT = 8.31447215;   // J/mol/K

// MicroSD card log file
File logfile;
bool card_present = false;

// Display controller
auto display = ssd1351::SSD1351<Color,Buffer,DISP_H,DISP_W>(DISP_CS, DISP_DC, DISP_RST);

// Sensors
VEML6075           veml6075  = VEML6075();
Adafruit_MLX90614  mlx90614  = Adafruit_MLX90614();
ClosedCube_HDC1080 hdc1080   = ClosedCube_HDC1080();
LSM9DS0            lsm9ds0   = LSM9DS0(MODE_I2C, I2C_ADDR_LSM9DS0_G, I2C_ADDR_LSM9DS0_XM);
MS5611             ms5611    = MS5611();
TCS3400            tcs3400   = TCS3400();
Adafruit_SGP30     sgp30     = Adafruit_SGP30();
TinyGPS            gps       = TinyGPS();

// Sensor values, current and last different value. The later is used to forego
// screen updates (which are unfortunately slow) if the value has not changed.

typedef struct sensval {
  time_t now;                 // (RTC/GPS) UNIX epoch timestamp (seconds)
  uint16_t year;              // (RTC/GPS) full 4 digit year (presume Gregorian)
  uint8_t month;              // (RTC/GPS) month (1-12(?))
  uint8_t day;                // (RTC/GPS) day (1-31(?))
  uint8_t hour;               // (RTC/GPS) hours (0-23(?))
  uint8_t minute;             // (RTC/GPS) minutes (0-59(?))
  uint8_t second;             // (RTC/GPS) seconds (0-59(?))
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
  uint32_t age = 0;           // (gps) time since last position fix
  float alt = -1.0;           // (ms5611) altitude (m)
  float irtemp = -1.0;        // (mlx90614) temp (remote)
  uint16_t color_r = 0xFFFF;  // (tcs3400) red
  uint16_t color_g = 0xFFFF;  // (tcs3400) green
  uint16_t color_b = 0xFFFF;  // (tcs3400) blue
  uint16_t vis = 0x0;         // (tcs3400) clear (visible)
  uint16_t ir = 0x0;          // (tcs3400) clear (visible)
  uint16_t eco2 = 0;          // (sgp30) estimated CO2
  uint16_t tvoc = 0;          // (sgp30) total VOCs
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

sensval_t curr;

// SGP30 baseline values & absolute humidity vars
uint16_t sgp30_baseline_eco2 = 0;
uint16_t sgp30_baseline_tvoc = 0;
uint16_t sgp30_abs_humidity = 0;

// Is current baseline data valid?
bool sgp30_baseline_valid = false;

// Timestamp of last baseline fetch
uint32_t sgp30_baseline_time = 0;

// If we're starting a new baseline, this is the timestamp of when we started.
// Need to wait 12 hours (according to the datasheet) before it's valid to
// save.
uint32_t sgq30_baseline_start = 0;

// If we've received data on the GPS UART port
volatile bool new_gps_data = false;

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
    display.setCursor((x), (y)); \
    display.print(curr.name, print_arg); \
  } while (0)

#define DISPLAY_READING_UNIT(x, y, print_arg, name, unit) \
  do { \
    display.setCursor((x), (y)); \
    display.print(curr.name, print_arg); \
    display.print(unit); \
  } while (0)

#define SERIALPRINT(...)   if (Serial) { Serial.print(__VA_ARGS__);   }
#define SERIALPRINTLN(...) if (Serial) { Serial.println(__VA_ARGS__); }

// Controls if GPS data is dumped over Serial for debugging purposes
#define DEBUG_GPS (0)

// Tempeture conversion macros
#define C_TO_F(x) ((x) * 9.0/5.0 + 32.0)
#define C_TO_K(x) ((x) + 273.15)
#define K_TO_C(x) ((x) - 273.15)

////////////////////////////////////////////////////////////////////////
// HALPING
////////////////////////////////////////////////////////////////////////

float getBattVoltage() {
  //float measured = analogRead(BATT_DIV);
  //return (measured / ((float)(1 << ANALOG_RES))) * 2.0 * 3.3;
  return 0.0;
}

// Loads baseline data from EEPROM
void loadBaseline() {
  uint8_t b0, b1, b2, b3;

  b3 = EEPROM.read(SGP30_EEPROM_ADDR_TIME);
  b2 = EEPROM.read(SGP30_EEPROM_ADDR_TIME+1);
  b1 = EEPROM.read(SGP30_EEPROM_ADDR_TIME+2);
  b0 = EEPROM.read(SGP30_EEPROM_ADDR_TIME+3);
  sgp30_baseline_time = (b3<<24) | (b2<<16) | (b1<<8) | b0;

  b1 = EEPROM.read(SGP30_EEPROM_ADDR_ECO2);
  b0 = EEPROM.read(SGP30_EEPROM_ADDR_ECO2+1);
  sgp30_baseline_eco2 = (b1<<8) | b0;

  b1 = EEPROM.read(SGP30_EEPROM_ADDR_TVOC);
  b0 = EEPROM.read(SGP30_EEPROM_ADDR_TVOC+1);
  sgp30_baseline_tvoc = (b1<<8) | b0;

}

// Save baseline data to EEPROM
void saveBaseline() {
  uint8_t b0, b1, b2, b3;

  b0 = (uint8_t)((sgp30_baseline_time)       & 0xFF);
  b1 = (uint8_t)((sgp30_baseline_time) >>  8 & 0xFF);
  b2 = (uint8_t)((sgp30_baseline_time) >> 16 & 0xFF);
  b3 = (uint8_t)((sgp30_baseline_time) >> 24 & 0xFF);
  EEPROM.write(b3, SGP30_EEPROM_ADDR_TIME);
  EEPROM.write(b2, SGP30_EEPROM_ADDR_TIME+1);
  EEPROM.write(b1, SGP30_EEPROM_ADDR_TIME+2);
  EEPROM.write(b0, SGP30_EEPROM_ADDR_TIME+3);

  b0 = (uint8_t)((sgp30_baseline_eco2)       & 0xFF);
  b1 = (uint8_t)((sgp30_baseline_eco2 >>  8) & 0xFF);
  EEPROM.write(b1, SGP30_EEPROM_ADDR_ECO2);
  EEPROM.write(b0, SGP30_EEPROM_ADDR_ECO2+1);

  b0 = (uint8_t)((sgp30_baseline_tvoc)       & 0xFF);
  b1 = (uint8_t)((sgp30_baseline_tvoc >>  8) & 0xFF);
  EEPROM.write(b1, SGP30_EEPROM_ADDR_TVOC);
  EEPROM.write(b0, SGP30_EEPROM_ADDR_TVOC+1);

}

//
// Annoyingly, the SGP30 humidity compensation must be specified in terms of
// absolute humidity (g/m^3) instead of relative humidity (%). Conversion using
// ideal gas law + formula for saturation of water vapor based on current
// temperature.
//
// Reference:
//https://carnotcycle.wordpress.com/2012/08/04/how-to-convert-relative-humidity-to-absolute-humidity/
//
uint16_t relativeToAbsoluteHumidity(float rh, float tempC) {
  float p;

  // Get maximum saturation pressure of water vapor at current temp
  p = 6.112 * pow(M_E, (17.67 / tempC) / (tempC + 243.5));

  // If we're at x% humidity, the water vapor pressure should be that
  // percentage of the saturation value.
  p = p * (rh / 100.0);

  // Use ideal gas law (PV=nRT) to infer the density of water in a cubic meter
  // of air. Assume v = 1 m^3, solve for n:
  //      P * V     P
  //  n = ----- = -----
  //      R * T   R * T
  float n = p / (GAS_CONSTANT * C_TO_K(tempC));

  // Finally, given n, calculate density:
  //                          1
  // density = v/m = --------------------
  //                 n * WATER_G_PER_MOLE
  //
  float density = 1 / (n * WATER_G_PER_MOLE);

  // Finally, convert the density into the weirdo format the SGP30 expects it
  // in (16bit, fixed precision 8.8)
  double i, f;
  uint8_t msb, lsb;

  f = modf(density, &i);
  msb = lrint(i) % 256;
  lsb = lrint(f * 256) & 0xFF;

  return (msb<<8) | lsb;
}

time_t getTeensy3Time(void) {
  return Teensy3Clock.get();
}

void displayLabels() {

  //            <x>, <y>, <label_text>
  DISPLAY_LABEL(  0,  16, F("RH"));                   // (hdc1080) relative humidity (%)
  DISPLAY_LABEL(  0,  24, F("Ta"));                   // (hdc1080) temp (ambient deg C)
  DISPLAY_LABEL(  0,  32, F("Tr"));                   // (mlx90614) temp (remote deg C)
  DISPLAY_LABEL(  0,  40, F("P"));                    // (ms5611) pressure (abs kPa)
  DISPLAY_LABEL(  0,  48, F("alt"));                  // (ms5611) altitude (m)
  DISPLAY_LABEL(  0,  56, F("IR"));                   // (tcs3400) IR
  DISPLAY_LABEL(  0,  64, F("VIS"));                  // (tcs3400) visible
  DISPLAY_LABEL(  0,  72, F("UVI"));                  // (veml6075) UV index
  DISPLAY_LABEL(  0,  80, F("UVA"));                  // (veml6075) UVA channel
  DISPLAY_LABEL(  0,  88, F("UVB"));                  // (veml6075) UVB channel
  DISPLAY_LABEL(  0, 104, F("Acc (g)"));              // (lsm9ds0) accelerometer (g)
  DISPLAY_LABEL(  0, 112, F("Mag (G)"));              // (lsm9ds0) magnetometer (gauss)
  DISPLAY_LABEL(  0, 120, F("Gyr (" STR_DEG "/s)"));  // (lsm9ds0) gyroscope (deg/s)
  DISPLAY_LABEL( 36,  96, F("X"));                    // (lsm9ds0) x-values
  DISPLAY_LABEL( 72,  96, F("Y"));                    // (lsm9ds0) y-values
  DISPLAY_LABEL(108,  96, F("Z"));                    // (lsm9ds0) z-values

  //                <x>, <y>, <text>, <color>
  DISPLAY_LABEL_RGB( 60,  16, F("R"), COLOR_RED);     // (tcs3400) red (word)
  DISPLAY_LABEL_RGB( 60,  24, F("G"), COLOR_GREEN);   // (tcs3400) green (word)
  DISPLAY_LABEL_RGB( 60,  32, F("B"), COLOR_BLUE);    // (tcs3400) blue (word)
                 
  //            <x>, <y>, <label_text>
  DISPLAY_LABEL( 60,  40, F("lat"));                  // (gps) latitude (deg)
  DISPLAY_LABEL( 60,  48, F("lon"));                  // (gps) longitude (deg)
  DISPLAY_LABEL( 60,  56, F("CO" SQUARED));           // (sgp30) estimated CO2
  DISPLAY_LABEL( 60,  64, F("VOC"));                  // (sgp30) total VOC
  DISPLAY_LABEL( 60,  72, F("rIR"));                  // (veml6075) raw IR channel
  DISPLAY_LABEL( 56,  80, F("rVIS"));                 // (veml6075) raw vis channel
  DISPLAY_LABEL( 56,  88, F("dark"));                 // (veml6075) raw dark channel

}

void displayValues_tcs3400() {
  // x, y, print_arg, $name
  DISPLAY_READING( 74,  16, DEC, color_r);
  DISPLAY_READING( 74,  24, DEC, color_g);
  DISPLAY_READING( 74,  32, DEC, color_b);
  DISPLAY_READING( 14,  56, DEC, ir);
  DISPLAY_READING( 14,  64, DEC, vis);
}

void displayValues_veml6075() {
  // x, y, print_arg, $name
  DISPLAY_READING( 14, 72, 2, uvi);
  DISPLAY_READING( 14, 80, 2, uva);
  DISPLAY_READING( 14, 88, 2, uvb);
  DISPLAY_READING( 74, 72, DEC, raw_ir_comp);
  DISPLAY_READING( 74, 80, DEC, raw_vis_comp);
  DISPLAY_READING( 74, 88, DEC, raw_dark);
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

void displayValues_sgp30() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT( 74, 56, DEC, eco2, " ppm");
  DISPLAY_READING_UNIT( 74, 64, DEC, tvoc, " ppb");
}

void displayValues_batt() {
  // x, y, print_arg, $name, unit
  DISPLAY_READING_UNIT( 106,   8, 2, batt_v, "V");
}

void displayValues_datetime() {
  // x, y, print_arg, $name, "unit"
  DISPLAY_READING(   0,   8, DEC, year);
  DISPLAY_LABEL  (  15,   8, F("-"));
  if (curr.month < 10) {
    DISPLAY_LABEL  (  19,   8, F("0"));
    DISPLAY_READING(  23,   8, DEC, month);
  } else {
    DISPLAY_READING(  19,   8, DEC, month);
  }
  
  DISPLAY_LABEL  (  27,   8, F("-"));
  if (curr.day < 10) {
    DISPLAY_LABEL  (  31,   8, F("0"));
    DISPLAY_READING(  35,   8, DEC, day);
  } else {
    DISPLAY_READING(  31,   8, DEC, day);
  }

  //DISPLAY_LABEL  (  39,   8, F("Z"));

  if (curr.hour < 10) {
    DISPLAY_LABEL  (  43,   8, F("0"));
    DISPLAY_READING(  47,   8, DEC, hour);
  } else {
    DISPLAY_READING(  43,   8, DEC, hour);
  }
  DISPLAY_LABEL  (  51,   8, F(":"));
  if (curr.minute < 10) {
    DISPLAY_LABEL  (  53,   8, F("0"));
    DISPLAY_READING(  57,   8, DEC, minute);
  } else {
    DISPLAY_READING(  53,   8, DEC, minute);
  }
  DISPLAY_LABEL  (  61,   8, F(":"));
  if (curr.second < 10) {
    DISPLAY_LABEL  (  63,   8, F("0"));
    DISPLAY_READING(  67,   8, DEC, second);
  } else {
    DISPLAY_READING(  63,   8, DEC, second);
  }
}

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {

  // Setup serial
  delay(200);
  if (Serial) {
    Serial.begin(115200);
    SERIALPRINTLN(F("Tricorder v2.0-teensy (Built " __DATE__ " " __TIME__")"));
  }

  // Setup RTC
  setSyncProvider(getTeensy3Time);
  if (timeStatus() != timeSet) {
    SERIALPRINTLN(F("RTC sync failed!"));
  } else {
    SERIALPRINTLN(F("system clock synced with RTC"));
  }

  // Setup ADC
  analogReadResolution(ANALOG_RES);

  // Init GPS (it's a bit more complicated)
  if (GPS_UART) {
    GPS_UART.begin(GPS_UART_BAUD);
    SERIALPRINTLN("GPS UART init complete");
  }

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
  SERIALPRINTLN("SD init complete");

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
  SERIALPRINTLN("display init complete");

  // Init I2C and then sensors on that bus
  Wire.begin();

  veml6075.begin();
  mlx90614.begin(); 
  hdc1080.begin(I2C_ADDR_HDC1080);

  uint16_t status = lsm9ds0.begin();
  if (status != 0x49D4) {
    SERIALPRINT(F("LSM9DS0 init failed! status="));
    SERIALPRINTLN(status, HEX);
  }

  if (!ms5611.begin()) {
    SERIALPRINT(F("MS5611 not found!"));
  }

  if (!tcs3400.begin()) {
    SERIALPRINT(F("TCS3400 not found!"));
  }

  if (!sgp30.begin()) {
    SERIALPRINT(F("SGP30 not found!"));
  } else {
    loadBaseline();
    if (sgp30_baseline_time == 0xFFFFFFFF) {
      sgp30_baseline_valid = false;
    } else {
      sgp30_baseline_valid = true;
      sgp30.setIAQBaseline(sgp30_baseline_eco2, sgp30_baseline_tvoc);
    }
  }
  SERIALPRINTLN("i2c init complete");

}

// Callback that fires when data is received on Serial3, where the GPS is
// hooked up
void serialEvent3(void) {
  while (GPS_UART.available()) {
    char c = GPS_UART.read();
    if (gps.encode(c)) {
      new_gps_data = true;
    }
#if (DEBUG_GPS)
    SERIALPRINT(c);
#endif
  }
}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {

  // Poll RTC for time (UTC)
  time_t now  = getTeensy3Time();
  curr.year   = year(now);
  curr.month  = month(now);
  curr.day    = day(now);
  curr.hour   = hour(now);
  curr.minute = minute(now);
  curr.second = second(now);

  // Get battery status
  curr.batt_v = getBattVoltage();
  curr.batt_ma = 0.0;
  
  // Poll VEML6075
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
  sgp30_abs_humidity = relativeToAbsoluteHumidity(curr.rh, curr.temp);

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

  // Poll GPS
  if (new_gps_data) {
    new_gps_data = false;
    gps.f_get_position(&curr.lat, &curr.lon, &curr.age);
    //gps.crack_datetime(
    //  &curr.year, &curr.month, &curr.day,
    //  &curr.hour, &curr.minute, &curr.second,
    //  NULL, /* hundredths */
    //  NULL  /* age */
    //);
  }

  // Poll SGP30

  // Display readings refresh
  display.fillScreen(COLOR_BLACK);
  displayLabels();
  displayValues_datetime();
  displayValues_hdc1080(); 
  displayValues_mlx90614();
  displayValues_veml6075();
  displayValues_tcs3400();
  displayValues_ms5611();
  displayValues_lsm9ds0();
  displayValues_gps();
  displayValues_sgp30();
  displayValues_batt();
  display.updateScreen();

  // Delay between data polls
  delay(500);

}
