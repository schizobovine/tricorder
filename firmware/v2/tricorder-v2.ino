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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>

#include <VEML6075.h>

#define DISP_RST_PIN (-1)
#define DISP_COLOR   WHITE
#define DISP_MODE    SSD1306_SWITCHCAPVCC
#define DISP_ADDR    0x3C

#define BATT_DIV_PIN 9

#define BLE_SPI_CS   8
#define BLE_SPI_IRQ  7
#define BLE_SPI_RST  4
#define BLE_SPI_VERB false

// Bluetooth LE modem
Adafruit_BluefruitLE_SPI modem(BLE_SPI_CS, BLE_SPI_IRQ, BLE_SPI_RST);

// Display controller
//Adafruit_SSD1306 display(DISP_RST_PIN);

// Sensors
VEML6075 veml6075 = VEML6075();

//Adafruit_BMP280    bmp280    = Adafruit_BMP280();
//Adafruit_HTU21DF   htu21df   = Adafruit_HTU21DF();
//Adafruit_LSM9DS0   lsm9ds0   = Adafruit_LSM9DS0();
//Adafruit_SI1145    si1145    = Adafruit_SI1145();
//Adafruit_TCS34725  tcs34725  = Adafruit_TCS34725();
//Adafruit_TSL2561   tsl2561   = Adafruit_TSL2561();
//Adafruit_TSL2591   tsl2591   = Adafruit_TSL2591();

////////////////////////////////////////////////////////////////////////
// HALPING
////////////////////////////////////////////////////////////////////////

float getBattVoltage() {
  float measured = analogRead(BATT_DIV_PIN);
  return measured * 2.0 * 3.3 / 1024;
}

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {

  // Maybe the display is being dumb and not replying fast enough?
  delay(200);
  
  // Init i2C
  Wire.begin();
  veml6075.begin();

  // Init display
  //display.begin(DISP_MODE, DISP_ADDR);
  //display.clearDisplay();
  //display.setTextColor(DISP_COLOR);
  //display.setTextSize(0);
  //display.print(F("Tricorder Mk I"));
  //display.dim(false);
  //display.setTextWrap(false);
  //display.display();

  // Init sensors
  //bmp280.begin();
  //htu21df.begin(); 
  //lsm9ds0.begin();
  //mlx90614.begin(); 
  //si1145.begin();
  //tcs34725.begin();
  //tsl2561.begin();
  //tsl2591.begin();

  // Serial barf
  //Serial.begin(9600);
  //while(!Serial)
  //  ;

  // Initialize BLE modem
  if (modem.begin(BLE_SPI_VERB)) {
    while (!modem.isConnected()) {
      delay(100);
    }
    modem.setMode(BLUEFRUIT_MODE_DATA);
  }

  // For zee debugging
  //pinMode(13, OUTPUT);  
  //delay(500);

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  
  uint32_t now = millis();

  veml6075.poll();
  float uva = veml6075.getUVA();
  float uvb = veml6075.getUVB();
  float uvi = veml6075.getUVIndex();

  modem.print(now);
  modem.print(F(" UVA = "));
  modem.print(uva);
  modem.print(F(" UVB = "));
  modem.print(uvb);
  modem.print(F(" UV Index = "));
  modem.println(uvi);

  delay(1000);

}
