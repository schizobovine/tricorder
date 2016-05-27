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
#include <Adafruit_SSD1351.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_TSL2591.h>
#include <Adafruit_VEML6070.h>

#define BATT_DIV_PIN A7

#define BLE_CS  8
#define BLE_IRQ 7
#define BLE_RST 4

#define DEFAULT_PRESSURE 1000.0

////////////////////////////////////////////////////////////////////////
// GLOBAL OBJECTS
////////////////////////////////////////////////////////////////////////

Adafruit_BluefruitLE_SPI ble(BLE_CS, BLE_IRQ, BLE_RST);

Adafruit_BMP280 bmp280;
Adafruit_BNO055 bno055;
Adafruit_MLX90614 mlx90614;
Adafruit_SHT31 sht31;
Adafruit_TCS34725 tcs34725;
Adafruit_TSL2591 tsl2591;
Adafruit_VEML6070 veml6070;

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
  bno055.begin();
  mlx90614.begin();
  sht31.begin();
  tcs34725.begin();
  tsl2591.begin();
  veml6070.begin(VEML6070_1_T);

  // Init serial for debugging BLE modem
  while (!Serial)
    delay(1);
  Serial.begin(115200);
  Serial.println("HAI");

  // Initialize BLE modem
  ble.begin(true);
  ble.echo(false);
  ble.info();

  // For zee debugging
  pinMode(13, OUTPUT);

}

////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////

void loop() {

  // START DEBUG CRAP

  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);

  uint32_t now = millis();
  Serial.println(now);

  // END DEBUG CRAP

  float bmp280_temp = bmp280.readTemperature();
  float press = bmp280.readPressure();
  float alt = bmp280.readAltitude(DEFAULT_PRESSURE);

  Serial.println("BMP280");
  Serial.print("T   = "); Serial.println(bmp280_temp);
  Serial.print("P   = "); Serial.print(press); Serial.println("Pa");
  Serial.print("Alt = "); Serial.print(alt); Serial.println("m");

  imu::Vector<3> euler = bno055.getVector(Adafruit_BNO055::VECTOR_EULER);
  float x = euler.x();
  float y = euler.y();
  float z = euler.z();
  uint8_t cal_sys, cal_gyro, cal_acc, cal_mag;
  bno055.getCalibration(&cal_sys, &cal_gyro, &cal_acc, &cal_mag);

  Serial.println("BNO055");
  Serial.print("X = "); Serial.println(x);
  Serial.print("Y = "); Serial.println(y);
  Serial.print("Z = "); Serial.println(z);
  Serial.print("Calibration (Sys)  = "); Serial.println(cal_sys);
  Serial.print("Calibration (Gyro) = "); Serial.println(cal_gyro);
  Serial.print("Calibration (Acc)  = "); Serial.println(cal_acc);
  Serial.print("Calibration (Mag)  = "); Serial.println(cal_mag);

  float ir_amb_c = mlx90614.readAmbientTempC();
  float ir_amb_f = mlx90614.readAmbientTempF();
  float ir_obj_c = mlx90614.readObjectTempC();
  float ir_obj_f = mlx90614.readObjectTempF();

  Serial.println("MLX90614");
  Serial.print("Ambient = "); Serial.print(ir_amb_c); Serial.println("C");
  Serial.print("          "); Serial.print(ir_amb_f); Serial.println("F");
  Serial.print("Object  = "); Serial.print(ir_obj_c); Serial.println("C");
  Serial.print("          "); Serial.print(ir_obj_f); Serial.println("F");

  float sht31_temp = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  Serial.println("SHT31");
  Serial.print("T = "); Serial.println(sht31_temp);
  Serial.print("RH% = "); Serial.println(humidity);

  uint16_t r, g, b, c;
  tcs34725.getRawData(&r, &g, &b, &c);
  uint16_t colorTemp = tcs34725.calculateColorTemperature(r, g, b);
  uint16_t lux = tcs34725.calculateLux(r, g, b);

  Serial.println("TCS34725");
  Serial.print("R = "); Serial.println(r);
  Serial.print("G = "); Serial.println(g);
  Serial.print("B = "); Serial.println(b);
  Serial.print("C = "); Serial.println(c);
  Serial.print("Color Temp = "); Serial.println(colorTemp);
  Serial.print("Lux = "); Serial.println(lux);

  uint16_t vis = tsl2591.getLuminosity(TSL2591_VISIBLE);
  uint16_t ir = tsl2591.getLuminosity(TSL2591_INFRARED);

  Serial.println("TSL2591");

  float uv = veml6070.readUV();

  Serial.println("VEML6070");
  Serial.print("UV = "); Serial.println(uv);

  delay(1000);

}
