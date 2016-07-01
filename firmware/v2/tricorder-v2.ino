/*
 * tricorder.ino
 *
 * Adafruit Feather-based, Bluetooth-enabled sensor thingy. Version 2: better,
 * strong, now with more calcium!
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
 * Hardware Revsion: Mk 2.0
 *
 */

//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#include <math.h>
//#include <avr/io.h>
//#include <avr/pgmspace.h>

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_BMP280.h>
//#include <ClosedCube_HDC1080.h>
#include <VEML6075.h>

#define DISP_DC      (15)
#define DISP_RST     (16)
#define DISP_CS      (17)
#define SD_CS        (18)
#define SD_CARDSW    (19)
#define SD_FILENAME  (F("tricorder.log"))

#define BATT_DIV     (9)

#define BLE_CS       (8)
#define BLE_IRQ      (7)
#define BLE_RST      (4)
#define BLE_VERBOSE  (false)

//
// 16-bit color is weird: 5 bits for R & B, but 6 for G? I guess humans are
// more sensitive to it...
//

#define COLOR_RED   (0xF800) // 0b1111100000000000 (5)
#define COLOR_GREEN (0x07E0) // 0b0000011111100000 (6)
#define COLOR_BLUE  (0x001F) // 0b0000000000011111 (5)

//
// Composite colors
//

#define COLOR_BLACK   (0x0000) // 0b0000000000000000 (  0%,   0%,   0%)
#define COLOR_MAGENTA (0xF81F) // 0b1111100000011111 (100%,   0%, 100%)
#define COLOR_ORANGE  (0xFD20) // 0b1111110100100000 (100%,  64%,   0%)
#define COLOR_YELLOW  (0xFFE0) // 0b1111111111100000 (100%, 100%,   0%)
#define COLOR_CYAN    (0x07FF) // 0b0000011111111111 (  0%, 100%, 100%)
#define COLOR_PURPLE  (0xA11E) // 0b1010000100011110 ( 62%,  12%,  94%)
#define COLOR_WHITE   (0xFFFF) // 0b1111111111111111 (100%, 100%, 100%)

// Bluetooth LE modem
Adafruit_BluefruitLE_SPI modem(BLE_CS, BLE_IRQ, BLE_RST);

// MicroSD card log file
File logfile;
bool card_present = false;

// Display controller
Adafruit_SSD1351 display(DISP_CS, DISP_DC, DISP_RST);

// Sensors
VEML6075           veml6075  = VEML6075();
Adafruit_BMP280    bmp280    = Adafruit_BMP280();
Adafruit_MLX90614  mlx90614  = Adafruit_MLX90614();

////////////////////////////////////////////////////////////////////////
// HALPING
////////////////////////////////////////////////////////////////////////

float getBattVoltage() {
  float measured = analogRead(BATT_DIV);
  return measured * 2.0 * 3.3 / 1024;
}

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {

  // Set CS pins as outputs and set all high by default
  pinMode(DISP_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(BLE_CS, OUTPUT);
  digitalWrite(DISP_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(BLE_CS, HIGH);

  // Initialize BLE modem
  modem.begin(BLE_VERBOSE);

  // Init display
  delay(100);
  display.begin();
  display.quickFill(COLOR_BLACK);
  display.setCursor(0, 0);
  display.setTextColor(COLOR_ORANGE);
  display.setTextSize(1);
  display.print("Tricorder Mk 2.0");

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

  // Init i2C
  Wire.begin();

  // Init sensors
  veml6075.begin();
  bmp280.begin();
  mlx90614.begin(); 

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  
  //
  // Sensor polling
  //

  uint32_t now = millis();

  veml6075.poll();
  float uva = veml6075.getUVA();
  float uvb = veml6075.getUVB();
  float uvi = veml6075.getUVIndex();

  //
  // BLE data output
  //
  if (modem.isConnected()) {
    modem.setMode(BLUEFRUIT_MODE_DATA);
    modem.print(now);
    modem.print(F(" UVA = "));
    modem.print(uva);
    modem.print(F(" UVB = "));
    modem.print(uvb);
    modem.print(F(" UV Index = "));
    modem.println(uvi);
  }

  //
  // Display refresh
  //
  display.quickFill(COLOR_BLACK);
  display.setTextSize(1);

  display.setTextColor(COLOR_ORANGE);
  display.setCursor(0, 0);
  display.print("Tricorder Mk 2.0");

  display.setTextColor(COLOR_WHITE);
  display.setCursor(0, 8);
  display.print(F("Time ="));
  display.setCursor(48, 8);
  display.print(now);

  display.setTextColor(COLOR_YELLOW);
  display.setCursor(0, 16);
  display.print(F(" UVA ="));
  display.setCursor(48, 16);
  display.print(uva);

  display.setTextColor(COLOR_RED);
  display.setCursor(0, 24);
  display.print(F(" UVB ="));
  display.setCursor(48, 24);
  display.print(uvb);

  display.setTextColor(COLOR_PURPLE);
  display.setCursor(0, 32);
  display.print(F(" UVI ="));
  display.setCursor(48, 32);
  display.print(uvi);

  //
  // Delay between data polls
  //
  delay(1000);

}
