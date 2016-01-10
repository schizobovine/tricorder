/*
 * tricorder.ino
 *
 * Adafruit Feather-based, Bluetooth-enabled sensor thingy.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HTU21DF.h>
#include <Adafruit_LSM9DS0.h>
#include <Adafruit_SI1145.h>
//#include <Adafruit_TSL2561.h>
//#include <Adafruit_TSL2591.h>

#define DISP_RST_PIN (-1)
#define DISP_COLOR   WHITE
#define DISP_MODE    SSD1306_SWITCHCAPVCC
#define DISP_ADDR    0x3C

// Display controller
Adafruit_SSD1306 display(DISP_RST_PIN);

// Sensors
Adafruit_SI1145 si1145 = Adafruit_SI1145();
Adafruit_BMP280 bmp280 = Adafruit_BMP280();

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {

  // Maybe the display is being dumb and not replying fast enough?
  delay(200);
  
  // Init i2C
  Wire.begin();

  // Init display
  display.begin(DISP_MODE, DISP_ADDR);
  display.clearDisplay();
  display.setTextColor(DISP_COLOR);
  display.setTextSize(0);
  display.print("Tricorder Mk I");
  display.dim(false);
  display.setTextWrap(false);
  display.display();

  // Init sensors
  si1145.begin();
  bmp280.begin();

  // Serial barf
  //Serial.begin(9600);
  //while(!Serial)
  //  ;

  // For zee debugging
  pinMode(13, OUTPUT);  
  delay(500);

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  
  //uint32_t now = millis();

  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("Tricorder Mk I");

  display.setCursor(0, 16);
  display.print("UV Idx ");
  display.print(((float)si1145.readUV()) / 100.0, 2);

  display.setCursor(0, 24);
  display.print("TempC ");
  display.print(bmp280.readTemperature(), 2);

  display.setCursor(0, 32);
  display.print("P(hPa) ");
  display.print(bmp280.readPressure() / 100.0, 2);

  display.setCursor(0, 40);
  display.print("Alt(m) ");
  display.print(bmp280.readAltitude(), 1);
  
  display.display();

  delay(1000);

}
