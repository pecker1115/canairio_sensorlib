/**
 * @file main.cpp
 * @author Antonio Vanegas @hpsaturn
 * @date June 2018 - 2020
 * @brief Particle meter sensor tests
 * @license GPL3
 * 
 * Full documentation:
 * https://github.com/kike-canaries/canairio_sensorlib#canairio-air-quality-sensors-library
 * 
 * Full implementation for WiFi and Bluetooth Air Quality fixed and mobile station:
 * https://github.com/kike-canaries/canairio_firmware#canairio-firmware
 * 
 * CanAirIO project docs:
 * https://canair.io/docs
 */

#include <Arduino.h>
#include <Sensors.hpp>

void onSensorDataOk() {
    Serial.print ("-->[MAIN] PM1: "+sensors.getStringPM1());
    Serial.print (" PM2.5: " + sensors.getStringPM25());
    Serial.print (" PM10: " + sensors.getStringPM10());
    
//    Serial.print (" CO2: " + sensors.getStringCO2());
//    Serial.print (" CO2humi: " + String(sensors.getCO2humi()));
//    Serial.print (" CO2temp: " + String(sensors.getCO2temp()));

    Serial.print (" H: "+ String(sensors.getHumidity()));
    Serial.println (" T: " + String(sensors.getTemperature()));
    
}

void onSensorDataError(const char * msg){ 
    Serial.println(msg);
}

/******************************************************************************
*  M A I N
******************************************************************************/

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n== Sensor test setup ==\n");

    Serial.println("-->[SETUP] Detecting sensors..");

    sensors.setSampleTime(5);                       // config sensors sample time interval
    sensors.setOnDataCallBack(&onSensorDataOk);     // all data read callback
    sensors.setOnErrorCallBack(&onSensorDataError); // [optional] error callback
    sensors.setDebugMode(true);                     // [optional] debug mode
    sensors.detectI2COnly(false);                   // disable force to only i2c sensors
    sensors.init();                                 // Auto detection to UART and i2c sensors

    // Alternatives only for UART sensors (TX/RX):

    // sensors.init(sensors.Auto);                  // Auto detection to UART sensors (Honeywell, Plantower, Panasonic)
    // sensors.init(sensors.Panasonic);             // Force UART detection to Panasonic sensor
    // sensors.init(sensors.Sensirion);             // Force UART detection to Sensirion sensor
    // sensors.init(sensors.Mhz19);                 // Force UART detection to Mhz14 or Mhz19 CO2 sensor
    // sensors.init(sensors.SDS011);                // Force UART detection to SDS011 sensor
    // sensors.init(sensors.CM1106);                // Force UART detection to CM1106 CO2 sensor
    // sensors.init(sensors.SENSEAIRS8);            // Force UART detection to SenseAirS8 CO2 sensor
    // sensors.init(sensors.Auto,PMS_RX,PMS_TX);    // Auto detection on custom RX,TX
  
    Serial.println("-->[SETUP] Sensor configured: " + sensors.getMainDeviceSelected());

    delay(500);
}

void loop() {
    sensors.loop();  // read sensor data and showed it
}