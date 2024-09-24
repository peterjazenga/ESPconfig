#ifndef ESPCONFIG_H
#define ESPCONFIG_H

// Include appropriate sysconfig file based on the architecture
#ifdef ESP32
    #include "sysconfig32/sysconfig32.h"
#elif defined(ESP8266)
    #include "sysconfig8266/sysconfig8266.h"
#endif

// Other shared code or declarations

#endif // ESPCONFIG_H