/*
 * tricorder.ino
 *
 * Adafruit Feather-based, Bluetooth-enabled sensor thingy.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
 *
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1351.h>
//#include <Adafruit_BLE.h>
//#include <Adafruit_BluefruitLE_SPI.h>

#include <Adafruit_Sensor.h>
//#include <Adafruit_BNO055.h>
//#include <Adafruit_MLX90614.h>
//#include <Adafruit_TCS34725.h>
//#include <Adafruit_TSL2591.h>
//#include <Adafruit_VEML6070.h>

#define BATT_DIV_PIN 9

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

  // Initialize BLE modem

  // For zee debugging
  pinMode(LED_BUILTIN, OUTPUT);  

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
