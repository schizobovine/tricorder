/*
 * tricorder.ino
 *
 * Adafruit Feather-based, Bluetooth-enabled sensor thingy.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
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
#include <Adafruit_SSD1306.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_HTU21DF.h>
#include <Adafruit_LSM9DS0.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SI1145.h>

#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>

#define DISP_RST_PIN (-1)
#define DISP_COLOR   WHITE
#define DISP_MODE    SSD1306_SWITCHCAPVCC
#define DISP_ADDR    0x3C

#define BATT_DIV_PIN 9

#define BLE_SPI_CS   8
#define BLE_SPI_IRQ  7
#define BLE_SPI_RST  4
#define BLE_SPI_VERB false

// Display controller
Adafruit_SSD1306 display(DISP_RST_PIN);

// Sensors
Adafruit_BMP280    bmp280    = Adafruit_BMP280();
Adafruit_HTU21DF   htu21df   = Adafruit_HTU21DF();
Adafruit_LSM9DS0   lsm9ds0   = Adafruit_LSM9DS0();
Adafruit_MLX90614  mlx90614  = Adafruit_MLX90614();
Adafruit_SI1145    si1145    = Adafruit_SI1145();
//Adafruit_TCS34725  tcs34725  = Adafruit_TCS34725();
//Adafruit_TSL2561   tsl2561   = Adafruit_TSL2561();
//Adafruit_TSL2591   tsl2591   = Adafruit_TSL2591();

// Bluetooth LE modem
Adafruit_BluefruitLE_SPI modem(BLE_SPI_CS, BLE_SPI_IRQ, BLE_SPI_RST);

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

  // Init display
  display.begin(DISP_MODE, DISP_ADDR);
  display.clearDisplay();
  //display.setTextColor(DISP_COLOR);
  //display.setTextSize(0);
  //display.print(F("Tricorder Mk I"));
  display.dim(false);
  display.setTextWrap(false);
  //display.display();

  // Init sensors
  bmp280.begin();
  htu21df.begin(); 
  lsm9ds0.begin();
  mlx90614.begin(); 
  si1145.begin();
  //tcs34725.begin();
  //tsl2561.begin();
  //tsl2591.begin();

  // Serial barf
  //Serial.begin(9600);
  //while(!Serial)
  //  ;

  // Initialize BLE modem
  modem.begin(BLE_SPI_VERB);

  // For zee debugging
  //pinMode(13, OUTPUT);  
  delay(500);

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  
  //uint32_t now = millis();

  display.clearDisplay();

  display.setCursor(0, 0);
  display.print(bmp280.readTemperature(), 2);
  display.print(F("\367C"));

  display.setCursor(64, 0);
  display.print(mlx90614.readObjectTempC(), 2);
  display.print(F("\367C"));

  display.setCursor(0, 8);
  display.print(F("RH "));
  display.print(htu21df.readHumidity(), 2);
  display.print(F("%"));

  display.setCursor(64, 8);
  display.print(bmp280.readPressure() / 100.0, 2);
  display.print(F(" hPa"));

  display.setCursor(0, 16);
  display.print(F("UV "));
  display.print(((float)si1145.readUV()) / 100.0, 2);

  display.setCursor(64, 16);
  display.print(F("Batt "));
  display.print(getBattVoltage(), 2);
  display.print(F("V"));

  lsm9ds0.read();

  // NB: I mounted the sensor wrong, so need to swap Y and Z axes, which is why
  // they're swapped in the code below.

  display.setCursor(0, 32); display.print(F("Acc"));
  display.setCursor(0, 40); display.print(F("X ")); display.print(lsm9ds0.accelData.x, 0);
  display.setCursor(0, 48); display.print(F("Y ")); display.print(lsm9ds0.accelData.z, 0);
  display.setCursor(0, 56); display.print(F("Z ")); display.print(lsm9ds0.accelData.y, 0);

  display.setCursor(64, 32); display.print(F("Mag"));
  display.setCursor(64, 40); display.print(F("X ")); display.print(lsm9ds0.magData.x, 0);
  display.setCursor(64, 48); display.print(F("Y ")); display.print(lsm9ds0.magData.z, 0);
  display.setCursor(64, 56); display.print(F("Z ")); display.print(lsm9ds0.magData.y, 0);
  
  display.display();

  delay(1000);

}
