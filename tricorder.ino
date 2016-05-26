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
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>

//#include <Adafruit_Sensor.h>
//#include <Adafruit_BNO055.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MLX90614.h>
//#include <Adafruit_VEML6070.h>

#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>

#define BATT_DIV_PIN 9

#define BLE_SPI_CS   8
#define BLE_SPI_IRQ  7
#define BLE_SPI_RST  4
#define BLE_SPI_VERB false

// Sensors
Adafruit_BMP280    bmp280    = Adafruit_BMP280();
Adafruit_MLX90614  mlx90614  = Adafruit_MLX90614();
//Adafruit_TSL2591   tsl2591   = Adafruit_TSL2591();
//Adafruit_TCS34725  tcs34725  = Adafruit_TCS34725();

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

  // Init sensors
  bmp280.begin();
  mlx90614.begin(); 
  //tcs34725.begin();
  //tsl2591.begin();

  // Serial barf
  /Serial.begin(9600);
  //while(!Serial)
  //  ;

  // Initialize BLE modem
  modem.begin(BLE_SPI_VERB);

  // For zee debugging
  pinMode(LED_BUILTIN, OUTPUT);  
  delay(500);

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  
  delay(1000);

}
