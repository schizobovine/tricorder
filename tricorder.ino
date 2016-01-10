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
#include <Adafruit_SI1145.h>


#define DISP_RST_PIN (-1)
#define DISP_COLOR   WHITE
#define DISP_MODE    SSD1306_SWITCHCAPVCC
#define DISP_ADDR    0x3C

// Display controller
Adafruit_SSD1306 display(DISP_RST_PIN);

// Sensors
Adafruit_SI1145 si1145 = Adafruit_SI1145();

////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////

void setup() {
  
  // Init i2C
  Wire.begin();

  // Init display
  display.begin(DISP_MODE, DISP_ADDR);
  display.clearDisplay();
  display.setTextColor(DISP_COLOR);
  display.setTextSize(0);
  display.print("Tricorder");
  display.dim(false);
  display.setTextWrap(false);
  display.display();

  // Serial barf
  //Serial.begin(9600);
  //while(!Serial)
  //  ;

  // For zee debugging
  pinMode(13, OUTPUT);  

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {
  
  uint32_t now = millis();
  display.clearDisplay();
  display.setCursor(0, 8);
  display.print(now);
  display.display();

  //Serial.println(now);

  digitalWrite(13, HIGH);
  delay(1000);
  digitalWrite(13, LOW);
  delay(1000);

}
