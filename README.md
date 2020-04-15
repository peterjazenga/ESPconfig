# ESPconfig
A configuration tool for ESP32 &amp; ESP8266 devices

I work with ESP32 & ESP8266 for various embedded projects and I have found the Arduino IDE to be rather primitive compared to almost every other IDE I have used however it has one huge advantage, it gives me an easy to use tool and common core for a wide range of SOC based boards with Atmel, ARM, ESP and several other processors along with very easy uploading to the target

The IDE is of course targeted at hobbyists so many of the libraries are written by non-professional coders leading to code quality and license issues for commercial developers
Some amateur code is very good unfortunately I would not class the available web based ESP configuration tools among them

As ESP devices are very low cost and include WiFi, they are strong candidates for low cost WiFi enabled IOT devices

It is because of this that I have decided to integrate various code chunks from a few different projects into a library that will address the config problem

I have the following goals

Ease of use
Persistent settings using a config file stored in SPIFFS
Time setting functions
OTA updates
User control
Online registration of the user & device
Shared login update across multiple devices on the same subnet
Ability to login to multiple WiFi access points
Hotspot, WiFi extender & Mesh modes
For ESP32, Bluetooth configuration
Extendable config pages with easy to use form building objects
Class based for ease of integration and use
Easy to brand
Help/About page supported and loaded from SPIFFS
Arduino provides an easy path to OTA but its on constantly making it a security risk and it does not offer and easy to use file uploader

Some of the existing config systems use EEPROM emulation on ESP devices which does not incorporate wear leveling leading to relatively low write count and reliability

None of the config systems I have found offer an easy to use time setting function with NTP support

None seem to offer even the most basic login security

This tool is being built to address these shortcomings
