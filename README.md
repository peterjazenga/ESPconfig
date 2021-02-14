# ESPconfig
A configuration tool for ESP32 &amp; ESP8266 devices

<p>I work with ESP32 &amp; ESP8266 for various embedded projects and I have found the Arduino IDE to be rather primitive compared to almost every other IDE I have used however it has one huge advantage, it gives me an easy to use tool and common core for a wide range of SOC based boards with Atmel, ARM, ESP and several other processors along with very easy uploading to the target</p>
<p>The IDE is of course targeted at hobbyists so many of the libraries are written by non-professional coders leading to code quality and license issues for commercial developers<br />
Some amateur code is very good, unfortunately I would not class the available web based ESP configuration tools among them</p>
<p>As ESP devices are very low cost and include WiFi, they are strong candidates for low cost WiFi enabled IOT devices</p>
<p>It is because of this that I have decided to integrate various code chunks from a few different projects into a library that will address the config problem</p>
<p>It is being developed with the following goals</p>
<ol>
<li>Ease of use</li>
<li>Persistent settings using a config file stored in SPIFFS ✅</li>
<li>Time setting functions  ✅</li>
<li>OTA updates ✅</li>
<li>User control ✅</li>
<li>Online registration of the user &amp; device</li>
<li>Shared login update across multiple devices on the same subnet</li>
<li>Ability to login to multiple WiFi access points</li>
<li>Hotspot, WiFi NAT extender</li>
<li>For ESP32, Bluetooth configuration ✅</li>
<li>Extendable config pages with easy to use form building objects</li>
<li>Class based for ease of integration and use ✅</li>
<li>Easy to brand</li>
<li>Help/About page supported and loaded from SPIFFS</li>
</ol>
<p>Arduino provides an easy path to OTA but its on constantly making it a security risk and it does not offer and easy to use file uploader</p>
<p>Some of the existing config systems use EEPROM emulation on ESP devices which does not incorporate wear leveling leading to relatively low write count and reliability issues</p>
<p>None of the config systems I have found offer an easy to use time setting function with NTP support</p>
<p>None seem to offer even the most basic login security</p>
<p>This work is FOSS under the Mozilla license.</p>
<p><a href="https://github.com/peterjazenga/ESPconfig" target="_blank" rel="noopener">https://github.com/peterjazenga/ESPconfig</a></p>
<p>&nbsp;</p>

Built against time library from https://github.com/PaulStoffregen/Time or http://playground.arduino.cc/Code/Time version 1.5.0
