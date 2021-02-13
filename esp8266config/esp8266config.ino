/*  author: Peter Dunne, +44 7932 676 847 peterjazenga@gmail.com
 *  purpose: to control a relay or SSR via radio signal or WiFi signal
 *  requirements: serial interface for radio, read only mode, initial design uses 433 mhz receiver
 *  WiFi interface to facilitate remote control using JSON packets
 *  Web interface to Configure WiFi interface and define control parameters for the radio transmitter device
 *  Bluetooth (ESP32 only) control and Configuration interface to phones/tablets/etc.
 *  
 *  This module imports code by Peter Dunne from previous ESP projects in power sensing, RC and on/off power switches
 *  
 *  sysConfig.init(); is called after Serial.begin(); and it handles all WiFi, OTA, NAT, Time setting and Bluetooth configuration
 *  WPS is also supported via sysConfig
 *  sysConfig.run(); keeps things updated, provides for servicing of the OTA & Web server issues among others, it also handles the blinking of the status LED
 */

#include "sysconfig8266.h";

void setup() {
  Serial.begin(115200);
  sysConfig.init();
  Serial.println("Ready.");  
 }

void loop() {
  sysConfig.run();
}

