/*  author: Peter Dunne, +44 7932 676 847 peterjazenga@gmail.com
 *  purpose: to provide an effective configuration portal and the basis of ESP32 & ESP8266 IOT projects
 *  Bluetooth (ESP32 only) control and Configuration interface to phones/tablets/etc.
 *  the CSS file used is w3.css as it is compact and free to use
 *  the js file w3.js provides enough functions to enable compact devices like the ESP32 & ESP8266 with reasonable web interfaces
 *  https://www.w3schools.com/w3js/default.asp
 *  https://www.w3schools.com/w3css/default.asp 
 *  https://www.w3schools.com/html/default.asp
 *  the author is not affiliated with w3schools however its a clear source of useful information on html, css and several programming languages
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <FS.h>         //Include File System Headers
#include <time.h>       // time() ctime()
#include <sys/time.h>   // struct timeval
#include <TimeLib.h>
#include <coredecls.h>  // settimeofday_cb()
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <dhcpserver.h>
#include <sntp.h>                       // sntp_servermode_dhcp()

/*
#include <TimeAlarms.h>
#include <TZ.h>
#include <Schedule.h>
#include <PolledTimeout.h>

*/////////////////////////////////////
#define NAPT 1000
#define NAPT_PORT 10
// utility routines

// define a structure for on/off timers to control the device activity
typedef struct {
  time_t startTime;
  time_t endTime;
  uint8_t dow;// used as a set of flags to define what day of the week its active
  // the msb is the enable bit for the timer
} ttimer;
typedef struct {
  byte startTime;
  byte endTime;
  byte dow;// used as a set of flags to define what day of the week its active
  // the msb is the enable bit for the timer
} tverifytimer;

typedef struct {
char WiFiname[32];
char WiFipassword[32];  
} WiFiParams;

typedef struct {
WiFiParams AccessPoints[5];
char APname[32];
char APpassword[32];
IPAddress ip;
IPAddress gateway;
IPAddress subnet;
byte APchannel;
char NTPserverValue[128];
char BLEname[32];
char BLEpassword[32];
byte ConfigPage;
byte BLEConfig; 
byte APconfig; 
byte sensorConfig;
byte timerConfig;
byte TZ; // defaults to GMT, handles time zone
float setPoint; // also used as setPoint for PID loops
float rangeValue;
time_t min_on;
time_t max_on;
time_t min_off;
byte OTA;
float iceGuard; //used by heating and cooling to prevent icing, must be >0 to be active
bool activeHigh; // heating activates when temperature drops below the lower trigger value
bool PIDcontrolled; // if running on a PID, the value used for setPoint is setPoint
ttimer timers[32]; // all timers default to inactive and zero values 
short  burncount;
time_t burntime;
char userName[32];
char userPass[32];
char userEmail[128];
char companyName[32];
char URL[128];
char NodeName[32];
//char bufferValue[32];
time_t regDate;
long regID;
long checksum;
} dataframe;

typedef struct {
  String css;
  String value;
  String placeholder;
  String label;
  String name;
  boolean required;
  String inlineJS; // this is what is included with the component on the page
  String functionalJS;// needs to be cleared each time as its meant to be added to the code library only once
  String hint; // adds an acronym object using the label as the key text
} htmlproperties;

// defines
#define NaN -1
#define NoString -2
#define Range -3
#define NotFound -4
#define OK 0

#define FAST_BLINK 200 // use for errors
#define SLOW_BLINK 500   // use for standard operations
#define PT_IDLE      0b10100000101000001010000010100000
#define PT_UPLOADING 0b10101010101010101010101010101010
#define PT_CONNECTED 0b10001000100010001000100010001000
#define PT_SCANNING  0b11010000110100001101000011010000
#define SCAN_PERIOD 5000
#define CONNECT_PERIOD 500

// global variables
IPAddress ip(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);
int APchannel = 11;
unsigned int localPort = 2390;      // local port to listen for UDP packets

// main classes for Configuration management, time management and NAT
enum errorclass {ecRange,ecRequired,ecNaN,ecPassword};
enum s_class {s_none, s_int, s_float, s_text};// used by the select functions

typedef std::function<void(time_t trigger)> TTimerFunction;
  
class tSysConfig {
 private:
  dataframe _data; // the data is contained in the private section as its not meant to be directly interatecd with by the main program
  dataframe _formdata; // the data contained herein is used to process the form data
  // LED information for heartbeat LED
  long lastBlinkMillis;
  int blinkstate=0;// defines which blink is used, 0-3 are slow 4-7 are fast blink
  int LEDstate=0;//inidicates which bit of the pattern is in use
  // WiFi connection information
  long currentMillis = 0;
  long lastScanMillis;
  long lastConnectMillis;
  // network scanning info
  bool STAconnected=false;
  int n=-2;
  String ssid;
  uint8_t encryptionType;
  int32_t RSSI;
  uint8_t* BSSID;
  int32_t channel;
  bool isHidden;
 protected: 
   uint32_t realSize;
  uint32_t ideSize;
  FlashMode_t ideMode;
  String error; // used singularly during form processing, each new element resets this
  int errorclass; // used singularly during form processing, each new element resets this
  int errorcount;
  String HTML; // used to build the web page
  String options; // used to build options lists during processing, this is necessary so that the error class for string lists can be defined during post processing
  char buffer[128];
  bool inForm;
  bool inFieldset;
  s_class inSelect; // if we are in a select group we need to know the class so it processes the correct information
  bool inOptgroup;
/*  int inDiv; // divisions can be nested
  bool inTable[4];// there are 5 flags corresponding to Table, THead, TR, TD & TFoot
  bool inUL;
  bool inOL;*/
  
 public:
   int configPin = D2;
 public:
// this is API to the Config system but the only routines that the user is required to utilize are tSysConfig(); & run();
// if desired, the device name can be changed prior to calling init
  htmlproperties webobj;
  ESP8266WebServer server;
  ESP8266WiFiMulti WiFiMulti;
  WiFiUDP udp;  
  void firstrun(); 
  /* this is meant to engage an automated Configuration mode that connects to a predefined WiFi and then calls some functions to get the logo, CSS, about, help and initConfig files 
  *  firstrun passes the device Mac number to the called program, this then sets a unique login ID for the device Configuration file  
  *  firstrun also exposes the AP with the default device name in the format of TRL-xxxxxx with the password of TRLinitialize
  */ 
  void initNTP(); // time system
  void initWiFi();
  void scanWiFi();
  void initWebserver(); // sets up the network and servers
  bool readConfig();// reads from the Configuration file, also enters system defaults to complete initialization routines
  void initConfig();// reads the Configuration file, also responsiible for calling firstrun if the Config files do not exist
  bool writeConfig();// writes to the Configuration file
  void updateConfig();// reinitialize the unit
  void OTAinit(); // Over the air update system management
  void OTArun();
  void blink(); 
  void init(); // startup code
  void run();  // called from the loop code to maintain periodic functions
  // there are lots of web functions as this is a web based Configuration program
  void indexPage(void);
  // procedures to handle forms
  boolean isInteger(String str); 
  boolean isFloat(String str);
  boolean isTime(String str);
  boolean isIP(String str);

  // copy values from post function is overloaded and has default parameters, if min-max are equal, range is ignored otherwise range checking is used
  int copyval(byte &var, char* name, byte min=0, byte max=0);
  int copyval(int &var, char* name, int min=0, int max=0);
  int copyval(float &var, char* name, float min=0, float max=0);
  int copyval(char* var, char* name, int size, int min=0);
  int copyval(time_t &var, char* name);
  int copyval(bool &var, char* name); // checkbox is unlike other fields, if its checked, it is included otherwise the field is not returned at all by the form post
  int copyIP(IPAddress &var, char* name);

  // form creation support
  bool form(htmlproperties obj);// all forms are post request only
  bool form(); // if the buttoncaption is not included, then the standard send button is created
  
  void fieldset(htmlproperties obj); // use to create groupboxes on the form
  void fieldset();

  bool edit(htmlproperties obj, char* data, int size, int min=0, int max=0);
  bool edit(htmlproperties obj, byte &data, byte min=0, byte max=0);
  bool edit(htmlproperties obj, int &data, int min=0, int max=0);
  bool edit(htmlproperties obj, float &data, float min=0.0, float max=0.0);
  bool edit(htmlproperties obj, time_t &data, time_t min=0, time_t max=0);
  // unlike other fields, password auto includes Label & verify dialog
  bool password(htmlproperties obj, char* data, int size, int min=0, int max=0);
  bool text(htmlproperties obj, char* data, int size, int min=0, int max=0);

  bool droplist(htmlproperties obj, float &data); 
  bool droplist(htmlproperties obj, int &data);
  bool droplist(htmlproperties obj, char* data, int size);// using this forces options to use the name field as value
  bool droplist();// terminates the list
  bool optiongroup(htmlproperties obj); //groups a value list
  bool optiongroup(); //terminates the group
  bool option(htmlproperties obj);
  bool option(htmlproperties obj, bool &data);
  bool option(htmlproperties obj, int &data);
  bool option(htmlproperties obj, float &data);
  bool option(htmlproperties obj, char* data);
  bool checkbox(htmlproperties obj, bool &data);
  bool checkbox(htmlproperties obj, int &data);
  bool checkbox(htmlproperties obj, char* data);
  bool radio(htmlproperties obj, int &data);

  // additional html elements of significant use to us
  // https://www.w3schools.com/tags/tag_meter.asp
  void meter(htmlproperties obj, int value, int min=0, int max=0);
  void meter(htmlproperties obj, float value, float min =0, float max =0);
  // https://www.w3schools.com/tags/tag_progress.asp
  void progress(htmlproperties obj, int value, int min=0, int max=0);
  void progress(htmlproperties obj, float value, float min =0, float max =0);
  // https://www.w3schools.com/tags/tag_details.asp
  void details(htmlproperties obj);
  void newURL(htmlproperties obj);// simply adds a URL to the page
  void menuItem(htmlproperties obj);// adds URL as a button item using w3c.css
  void div();
  void getHeader();
  void getFooter();
  // server page support functions
  void getAbout();
  void getCharts();// does the charting page
  void drawGraph();// draws the actual image 
  void Config();
  void Sensors();
  void Timers();
  void ACL(); 
  void Register();
  void MQTT();
  void Sysinfo();
  void getHelp();
  // binary
  void getCSS();
  void getLogo();
  void getIcon();
  void getJS();
  bool webAuth(); // used by restricted Configuration pages
  void handleNotFound();
  // to support a Configuration application, we have the JSON data exchanges, there are no individual pages in the JSON processing
  void JSON();// format bytes
  // additional utility routines
  String formatBytes(size_t bytes);
  
};


void tSysConfig::init() {
  WiFi.persistent(false);
  pinMode(LED_BUILTIN, OUTPUT);
  realSize = ESP.getFlashChipRealSize();
  ideSize = ESP.getFlashChipSize();
  ideMode = ESP.getFlashChipMode();
  Serial.printf(" ESP8266 Chip id = %08X\n\r", ESP.getChipId());
  Serial.printf("Flash real id:   %08X\n\r", ESP.getFlashChipId());
  Serial.printf("Flash real size: %u bytes\n\r", realSize);
  Serial.printf("Flash ide  size: %u bytes\n\r", ideSize);
  Serial.printf("Flash ide speed: %u Hz\n\r", ESP.getFlashChipSpeed());
  Serial.printf("Flash ide mode:  %s\n\r", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
  if (ideSize != realSize) {
    Serial.println("Flash Chip Configuration wrong!");
  } else {
    Serial.println("Flash Chip Configuration ok.");
  } 
  Serial.print("Compilation date:");
  Serial.print(__DATE__);
  Serial.print(" @ ");
  Serial.println(__TIME__);
  
  readConfig();
  initConfig();
  initWiFi();
  initWebserver();
  initNTP();
  OTAinit(); // OTA update services
  
}
void tSysConfig::run() {
  // as there is no stop or end function in ArduinoOTA, this hack is used to stop it functioning
  // there is no provision for on demand update in the code yet
  // noob value is to activate OTA
  switch (_data.OTA){
    case 2:if (millis()<300000){ ArduinoOTA.handle();} break; // active on boot for 5 minutes
    case 4:break; // totally stopped
    default: ArduinoOTA.handle();
  }
  server.handleClient();
  MDNS.update();  
  currentMillis = millis();
  
  blink();  
}

void tSysConfig::initNTP() {
  
}

void tSysConfig::initWebserver() {

  server.on("/", [&]{ indexPage(); });
  server.on("/charts.html", [&]{ getCharts();} );
  server.on("/graph", [&]{ drawGraph();} );
  server.on("/config.html", [&]{ Config(); });
  server.on("/sensor.html", [&]{ Sensors(); });
  server.on("/timers.html", [&]{ Timers(); });
  server.on("/acl.html", [&]{ ACL(); });
  server.on("/register.html", [&]{ Register(); });
  server.on("/mqtt.html", [&]{ MQTT(); });
  server.on("/sysinfo.html", [&]{ Sysinfo(); });
  server.on("/help.html", [&]{ getHelp(); });
  server.on("/about.html", [&]{ getAbout(); });
  /*  */
  server.on("/favicon.ico", [&] { getIcon();} );
  server.on("/logo.png", [&]{ getLogo();} );
  server.on("/w3.css", [&]{ getCSS();} );
  server.on("/w3.js", [&]{ getJS();} );
  server.onNotFound([&](){ handleNotFound(); });
  server.begin();
  
}

bool tSysConfig::readConfig(){
// read the parameters from flash
  Serial.println("Opening SPIFFS /Config.bin");
   File ConfigFile = SPIFFS.open("/Config.bin", "r");
  if (!ConfigFile) {
    Serial.println("Failed to open Config file, attempting to load manufacturer's configuration");
    ConfigFile = SPIFFS.open("/defaults.bin", "r");
    if (!ConfigFile) {Serial.println("No manufacturers config file"); return false;}
  }
size_t size = ConfigFile.size();
  if (size != sizeof(_data)) {
    Serial.println("Config file size is invalid");
    return false;
  }
  ConfigFile.read((byte*) &_data, sizeof(_data));
  ConfigFile.close();
  if (_data.OTA==3){_data.OTA=4;}// ensures OTA is off at reboot
 return true;
}

void tSysConfig::initConfig() {
  Serial.println("initConfig()");
  if(!SPIFFS.begin())
  {
    Serial.println("SPIFFS Initialization...failed");
  } else { 
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.println();
    readConfig();
    Serial.print("Last Config:");
    Serial.println(_data.burntime);
    Serial.print("Burn count:");
    Serial.println(_data.burncount);
  }
  if (_data.burncount == 0) {
    Serial.print("Init failure: ");
    // initialize the flash memory
    strlcpy(_data.AccessPoints[0].WiFiname,"jazenga",32);
    strlcpy(_data.AccessPoints[0].WiFipassword,"FVLICEYE",32);
    strlcpy(_data.APname,"TRL_Device",32);
    strlcpy(_data.APpassword,"12345678",32);
// Set a static ip address for the Access point mode
// This can be used to directly connect to it instead of having to have a local network
    _data.ip = ip;
    _data.gateway = gateway;
    _data.subnet = subnet;
    _data.APchannel = APchannel;
    strlcpy(_data.NTPserverValue,"ntp.pool.org",32);
    strlcpy(_data.NTPserverValue,"north-america.pool.ntp.org",32);
    strlcpy(_data.BLEname,"TRL_Device",32);
    strlcpy(_data.BLEpassword,"1111",32);
    _data.ConfigPage=1;// always available
    _data.BLEConfig=1; // always on
    _data.APconfig=1; // softap is always available
    _data.sensorConfig=2; // heating
    _data.timerConfig=2; //defaults to timer only mode
    _data.TZ=0; // defaults to GMT
    _data.setPoint=0; // also used as setPoint for PID loops
    _data.rangeValue=0;
    _data.iceGuard=0; //used by heating and cooling to prevent icing, must be >0 to be active
    _data.activeHigh=false; // heating activates when temperature drops below the lower trigger value
    _data.PIDcontrolled=true; // if running on a PID, the value used for setPoint is setPoint*/
    // initialize the timers to zero hours on zero days
    Serial.println("Default parameters loaded");
  //  firstrun();
}  
}
void tSysConfig::initWiFi(){
  int retries = 0;
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  // scan for available networks
  scanWiFi();
  for ( uint8_t i = 0; i < 5; i++ ) {
   WiFiMulti.addAP(_data.AccessPoints[i].WiFiname, _data.AccessPoints[i].WiFipassword);
  }
  WiFi.beginSmartConfig();

 // WiFi.begin(_data.AccessPoints[0].WiFiname, _data.AccessPoints[0].WiFipassword); 
  // wait for connection here
   while ((WiFiMulti.run() != WL_CONNECTED) && (retries<30)) {
    delay(500);
    Serial.print(".");
    ++retries;
  }
  if (WiFi.status() == WL_CONNECTED){
  STAconnected=true;
  Serial.println("");
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  Serial.println(F("Starting Time UDP"));
  udp.begin(localPort);
  Serial.print(F("Local port: "));
  Serial.println(udp.localPort());
  }
  else{
    Serial.println(F("WiFi failed to connect"));
  }
  WiFi.softAPConfig(_data.ip,_data.gateway,_data.subnet);
  // check if wifi is connected and what the AP rules are
  // Turn on local Access Point
  switch (_data.APconfig){
    case 0: break;
    case 1:{}
    case 2:{// enables softap using user IP settings, it has no code as the execution simply flows on to case 3:
    }
    case 3: {// enable AP, with android-compatible google domain  
      if (_data.APconfig==3){
            WiFi.softAPConfig(  
              IPAddress(172, 217, 28, 254),
              IPAddress(172, 217, 28, 254),
              IPAddress(255, 255, 255, 0));
            }
            // give DNS servers to AP side
            dhcps_set_dns(0, WiFi.dnsIP(0));
            dhcps_set_dns(1, WiFi.dnsIP(1));
            WiFi.softAP(_data.APname, _data.APpassword, _data.APchannel);
            err_t ret = ip_napt_init(NAPT, NAPT_PORT);
            if (ret == ERR_OK) {
              ret = ip_napt_enable_no(SOFTAP_IF, 1);
              if (ret == ERR_OK) {
              }
           }
         break;
    }
    case 4: {}// mesh mode
  }
if (WiFi.status() != WL_CONNECTED){ 
     WiFi.softAP(_data.APname, _data.APpassword, _data.APchannel); 
     Serial.println(F("WiFi AP started"));}  
}

bool tSysConfig::writeConfig() {
  File configFile = SPIFFS.open("/config.bin", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  ++_data.burncount;
  _data.burntime=time(nullptr);
  configFile.write((byte*) &_data, sizeof(_data));
  configFile.close();
  return readConfig();
}

void tSysConfig::updateConfig() {
  
}

void tSysConfig::blink() {
  int t=SLOW_BLINK;
if (blinkstate>3) {t=FAST_BLINK;}
  if (currentMillis - lastBlinkMillis > t)
  {
    LEDstate++;
    switch (blinkstate){
      case 0:
      case 4:{digitalWrite(LED_BUILTIN,(bool) PT_IDLE>>LEDstate ? true : false); break;}
      case 5:
      case 1:{digitalWrite(LED_BUILTIN,(bool) PT_UPLOADING>>LEDstate ? true : false); break;}
      case 6:
      case 2:{digitalWrite(LED_BUILTIN,(bool) PT_CONNECTED>LEDstate ? true : false); break;}
      case 7:
      case 3:{digitalWrite(LED_BUILTIN,(bool) PT_SCANNING>>LEDstate ? true : false); break;}
    }
    lastBlinkMillis = currentMillis;
  }
}
void tSysConfig::OTAinit(){ 
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", progress / (total / 100));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(_data.APname);
  ArduinoOTA.begin(); 
  }
void tSysConfig::OTArun(){
  switch (_data.OTA){
    case 2:if (millis()<300000){ ArduinoOTA.handle();} break; // active on boot for 5 minutes
    case 4:break; // totally stopped
    default: ArduinoOTA.handle();
  }
  server.handleClient();
  MDNS.update();
}

void tSysConfig::indexPage(void) {
  Serial.println("doing index");
  File dataFile = SPIFFS.open("/index.html", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "text/html") != dataFile.size()) {Serial.println(F("index.html streaming error"));
  }
  dataFile.close();   
}
void tSysConfig::Config(){ 
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Config Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("System configuration <button class=\"w3-bar-item w3-button\" onclick=\"openTab('A')\">WiFi connection</button>");
  HTML+=F("<button class=\"w3-bar-item w3-button\" onclick=\"openTab('B')\">AP</button><button class=\"w3-bar-item w3-button\" onclick=\"openTab('C')\">Bluetooth</button>");
  HTML+=F("<button class=\"w3-bar-item w3-button\" onclick=\"openTab('D')\">Time</button></div>");

  HTML+=F("</body></html>");
}
void tSysConfig::Sensors(){
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Sensor Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("Sensor configuration</div>");

  HTML+=F("</body></html>");
  server.send(200, "text/html", HTML);
}
void tSysConfig::Timers(){ 
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("Timers</div>");

  HTML+=F("</body></html>"); 
  server.send(200, "text/html", HTML);
}

void tSysConfig::ACL(){ 
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("Access control</div>");

  HTML+=F("</body></html>"); 
}
void tSysConfig::Register(){ 
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("Device registration</div>");

  HTML+=F("</body></html>"); 
  server.send(200, "text/html", HTML);
}
void tSysConfig::MQTT(){ 
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("MQTT configuration</div>");

  HTML+=F("</body></html>"); 
  server.send(200, "text/html", HTML);
}

void tSysConfig::Sysinfo(){ 
  HTML=F("<!DOCTYPE html><html><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  HTML+=F("<link rel=\"stylesheet\" href=\"w3.css\"><script src=\"w3.js\"></script><body><div class=\"w3-container w3-blue\">");
  HTML+=F("Sysinfo</div><table class=\"w3-table w3-striped w3-border\"><tr><th>Parameter</th><th>Value</th></tr>");
  sprintf(buffer," <tr><td>ESP 8266 Chip id</td><td>%08X</td></tr>",ESP.getChipId());
  HTML+=buffer;
  sprintf(buffer," <tr><td>Flash id</td><td>%08X</td></tr>",ESP.getFlashChipId());
  HTML+=buffer;
  sprintf(buffer," <tr><td>Flash real size</td><td>%u</td></tr>",realSize);
  HTML+=buffer;
  sprintf(buffer," <tr><td>Flash real speed</td><td>%u</td></tr>",ideSize);
  HTML+=buffer;
  sprintf(buffer," <tr><td>Flash IDE speed</td><td>%u Hz</td></tr>",ESP.getFlashChipSpeed());
  HTML+=buffer;
  sprintf(buffer," <tr><td>Flash IDE size</td><td>%u</td></tr>",ESP.getFlashChipSpeed());
  HTML+=buffer;
  sprintf(buffer," <tr><td>Flash mode</td><td>%s</td></tr>",(ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
  HTML+=buffer;
  sprintf(buffer," <tr><td>Compilation date</td><td>%s @ %s</td></tr>",__DATE__,__TIME__);
    HTML+=F("<tr><td>Flash Chip Configuration</td>");
   if (ideSize != realSize) {    HTML+=F("<td class=\"w3-red\">wrong!.</td></tr>");
  } else {
    HTML+=F("<td>ok.</td></tr>");
  } 
  sprintf(buffer," <tr><td>Connected to</td><td>%s</td></tr>",WiFi.SSID().c_str());
  HTML+=buffer;
  IPAddress IP = WiFi.localIP();
  IPAddress GW = WiFi.gatewayIP();
  char buf[20];
  sprintf(buf, "%d.%d.%d.%d<br><a href=\"%d.%d.%d.%d\" target=\"_blank\">%d.%d.%d.%d</a>", IP[0],IP[1],IP[2],IP[3], GW[0],GW[1],GW[2],GW[3], GW[0],GW[1],GW[2],GW[3] );
  sprintf(buffer,"<tr><td>IP address<BR>Gateway</td><td>%s</td></tr>",buf);
  HTML+=buffer;
  sprintf(buffer," <tr><td>Access point name</td><td>%s</td></tr>",_data.APname);
  HTML+=buffer;
  IP = WiFi.softAPIP();
  sprintf(buf, "%d.%d.%d.%d", IP[0],IP[1],IP[2],IP[3] );
  sprintf(buffer,"<tr><td>IP address</td><td>%s</td></tr>",buf);
  HTML+=buffer;
  
 HTML+=F("</body></html>");
 server.send(200, "text/html", HTML); 
}

void tSysConfig::getIcon(){
  Serial.println("doing Favicon.ico");
  File dataFile = SPIFFS.open("/favicon.ico", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "image/vnd") != dataFile.size()) {Serial.println(F("icon streaming error"));
  }
  dataFile.close();   
}
void tSysConfig::getLogo(){
  Serial.println("doing logo");
  File dataFile = SPIFFS.open("/logo.png", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "image/png") != dataFile.size()) {Serial.println(F("Logo streaming error"));
  }
  dataFile.close();   
}
void tSysConfig::getCSS(){
  Serial.println("doing css");
  File dataFile = SPIFFS.open("/w3.css", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "text/css") != dataFile.size()) {Serial.println(F("Logo streaming error"));
  }
  dataFile.close();   
}
void tSysConfig::getJS(){
  Serial.println("doing Javascript");
  File dataFile = SPIFFS.open("/w3.js", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "text/javascript") != dataFile.size()) {Serial.println(F("Logo streaming error"));
  }
  dataFile.close();    
}
void tSysConfig::getCharts(){
  
}
void tSysConfig::drawGraph(){ 
  
}
void tSysConfig::getAbout(){
  Serial.println("doing Aboout");
  File dataFile = SPIFFS.open("/about.html", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "text/html") != dataFile.size()) {Serial.println(F("Logo streaming error"));
  }
  dataFile.close();    
}
void tSysConfig::getHelp(){
  Serial.println("doing Help");
  File dataFile = SPIFFS.open("/help.html", "r"); 
  if (dataFile.size()<=0) {Serial.println("bad file size");
  }
  if (server.streamFile(dataFile, "text/html") != dataFile.size()) {Serial.println(F("Logo streaming error"));
  }
  dataFile.close();    
}
void tSysConfig::handleNotFound(){
  
}
void tSysConfig::getFooter(){
    // used by every page
  }
void tSysConfig::getHeader(){
    // used by every page
  }
bool tSysConfig::webAuth(){
    // used by restricted Configuration pages
  }
  // to support a Configuration application, we have the JSON data exchanges, there are no individual pages in the JSON processing
void tSysConfig::JSON(){
    // format bytes
    // must contain 2 parts, the post function and the get function
  }
// form creation support
bool tSysConfig::form(htmlproperties obj){
   // all forms are post request only
  }
bool tSysConfig::form(){
  // if the buttoncaption is not included, then the standard send button is created
  }
void tSysConfig::fieldset(htmlproperties obj){
  // use to create groupboxes on the form
  if (inFieldset){ fieldset();}
}
void tSysConfig::fieldset(){
    if (inFieldset){
      inFieldset=false;
      HTML+=F("</fieldset>");
    }
  }
bool tSysConfig::edit(htmlproperties obj, char* data, int size, int min, int max){
    
  }
bool tSysConfig::edit(htmlproperties obj, byte &data, byte min, byte max){
    
  }
bool tSysConfig::edit(htmlproperties obj, int &data, int min, int max){
    
  }
bool tSysConfig::edit(htmlproperties obj, float &data, float min, float max){
    
  }
bool tSysConfig::edit(htmlproperties obj, time_t &data, time_t min, time_t max){
    
  }
bool tSysConfig::password(htmlproperties obj, char* data, int size, int min, int max){
  // unlike other fields, password auto includes Label & verify dialog  
}
bool tSysConfig::text(htmlproperties obj, char* data, int size, int min, int max){
    
  }
bool tSysConfig::droplist(htmlproperties obj, int &data){
    droplist(); // ensure prevous selects are closed
    inSelect=s_int;
    options="";
    
  }
bool tSysConfig::droplist(htmlproperties obj, float &data){
    droplist(); // ensure prevous selects are closed
    inSelect=s_float;
    options="";
    
  }
bool tSysConfig::droplist(htmlproperties obj, char* data, int size){
    // using this forces options to use the name field as value
    droplist(); // ensure prevous selects are closed
    inSelect=s_text;
    options="";
  }
bool tSysConfig::droplist(){
    // ends the droplist
   if (inSelect!=s_none){
    optiongroup();// ensure option groups are closed
    // write the select code here
   HTML+=options;
   HTML+=F("</select>"); 
   }
  }
bool tSysConfig::optiongroup(htmlproperties obj){
  optiongroup();// close any previous group
  inOptgroup=true;
   sprintf(buffer,"<optgroup %s label=\"%s\">",obj.css.c_str(),obj.value.c_str()); 
   options+=buffer;    
  }
bool tSysConfig::optiongroup(){
   if (inOptgroup){
    inOptgroup=false;
   options+="</optgroup>";}    
  }
bool tSysConfig::option(htmlproperties obj, char *data){
  if (obj.value=data){
   sprintf(buffer,"<option value=\"%s\" %s selected>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.label.c_str());}else{
   sprintf(buffer,"<option value=\"%s\" %s>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.label.c_str()); 
   }
   options+=buffer;    
  }
bool tSysConfig::option(htmlproperties obj, float &data){
  if (obj.value.toFloat()==data){
   sprintf(buffer,"<option value=\"%s\" %s selected>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.label.c_str());}else{
   sprintf(buffer,"<option value=\"%s\" %s>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.label.c_str()); 
   }
   options+=buffer;    
  }
bool tSysConfig::option(htmlproperties obj, int &data){
  if (obj.value.toInt()==data){
   sprintf(buffer,"<option value=\"%s\" %s selected>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.label.c_str());}else{
   sprintf(buffer,"<option value=\"%s\" %s>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.label.c_str()); 
   }
   options+=buffer;    
  }
bool tSysConfig::checkbox(htmlproperties obj, bool &data){
   sprintf(buffer,"<option value=\"%s\" %s>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.name.c_str()); 
  HTML+=buffer;    
    
  }
bool tSysConfig::checkbox(htmlproperties obj, int &data){
   sprintf(buffer,"<option value=\"%s\" %s>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.name.c_str()); 
  HTML+=buffer;    
    
  }
bool tSysConfig::checkbox(htmlproperties obj, char* data){
    
  }
bool tSysConfig::radio(htmlproperties obj, int &data){
   sprintf(buffer,"<option value=\"%s\" %s>%s</option>",obj.value.c_str(),obj.css.c_str(),obj.name.c_str()); 
  HTML+=buffer;    
    
  }
  // additional html elements of significant use to us
void tSysConfig::meter(htmlproperties obj, int value, int min, int max){
    // name is not required as this item does not return information
   if (obj.label.length()>0){
   sprintf(buffer,"<label for=\"%s\">%s</label><meter id=\"%s\" value=\"%d\" max=\"%d\"> %d% </meter>",obj.name.c_str(), obj.label.c_str(), obj.name.c_str(), value, min, max );} else {
   sprintf(buffer,"<meter id=\"%s\" value=\"%d\" max=\"%d\"> %d% </meter>",obj.name.c_str(), value, min, max );} 
   HTML+=buffer; 
  }
void tSysConfig::meter(htmlproperties obj, float value, float min, float max){
    // name is not required as this item does not return information
   if (obj.label.length()>0){
   sprintf(buffer,"<label for=\"%s\">%s</label><meter id=\"%s\" value=\"%f\" max=\"%f\"> %f% </meter>",obj.name.c_str(), obj.label.c_str(), obj.name.c_str(), value, min, max );} else {
   sprintf(buffer,"<meter id=\"%s\" value=\"%f\" max=\"%f\"> %f% </meter>", obj.name.c_str(), value, min, max );}
   HTML+=buffer; 
  }
void tSysConfig::progress(htmlproperties obj, int value, int min, int max){
    // name is not required as this item does not return information
   if (obj.label.length()>0){
   sprintf(buffer,"<label for=\"%s\">%s</label><progress id=\"%s\" value=\"%d\" max=\"%d\"> %d% </progress>",obj.name.c_str(), obj.label.c_str(), obj.name.c_str(), value, min, max );} else {
   sprintf(buffer,"<progress id=\"%s\" value=\"%d\" max=\"%d\"> %d% </progress>", obj.name.c_str(), value, min, max );}
   HTML+=buffer; 
  }
void tSysConfig::progress(htmlproperties obj, float value, float min, float max){
    // name is not required as this item does not return information
   if (obj.label.length()>0){
   sprintf(buffer,"<label for=\"%s\">%s</label><progress id=\"%s\" value=\"%f\" max=\"%f\"> %f% </progress>",obj.name.c_str(), obj.label.c_str(), obj.name.c_str(), value, min, max );} else {
   sprintf(buffer,"<progress id=\"%s\" value=\"%f\" max=\"%f\"> %f% </progress>",obj.name.c_str(), value, min, max );}
   HTML+=buffer; 
  }
void tSysConfig::details(htmlproperties obj){
    // name is not required as this item does not return informationhttps://www.w3schools.com/tags/tag_button.asp
   sprintf(buffer,"<details %s><summary>%s</summary><p>%s</p></details>",obj.css.c_str(), obj.label.c_str(), obj.value.c_str() );
   HTML+=buffer; 
  }

void tSysConfig::newURL(htmlproperties obj){
    // simply adds a URL to the page
    // this has no post function
    if (obj.value.length()>0){
    sprintf(buffer,"<a href=\"%s\" target=\"%s\" %s %s>%s</a>/r/n",obj.name.c_str(),obj.value.c_str(),obj.inlineJS.c_str(), obj.css.c_str(), obj.label.c_str());} else {
    sprintf(buffer,"<a href=\"%s\" %s %s>%s</a>/r/n",obj.name.c_str(), obj.inlineJS.c_str(), obj.css.c_str(), obj.label.c_str());} 
    HTML+=buffer;
  }
void tSysConfig::menuItem(htmlproperties obj){
    // adds URL as a button item using w3c.css
    // this has no post function
    sprintf(buffer,"<button %s onclick=\"%s\" %s>%s</button>/r/n" ,obj.css.c_str(), obj.value.c_str(), obj.inlineJS.c_str(), obj.label.c_str()); 
    HTML+=buffer;
  }

// format bytes
String tSysConfig::formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

boolean tSysConfig::isInteger(String str){ // check the input is an integer
   for(byte i=0;i<str.length();i++){
      if(!isDigit(str.charAt(i))) {
        return false;
      }
   }
  return true;
} 
boolean tSysConfig::isFloat(String str){ // check that the input is a floating point number
  String s;
  String x;
  int pos = str.indexOf(".");
  if (pos==-1){ Serial.println("simple integer"); return isInteger(str); } else {
   s=str.substring(0,pos);
   x=str.substring(pos+1);
   if (isInteger(s)){return isInteger(x);} 
  }
 return false;
} 
boolean tSysConfig::isTime(String str){ // check that the input is a floating point number
  String s;
  String x;
  int pos = str.indexOf(":");
  if (pos==-1){ Serial.println("simple integer"); return false; } else {
   s=str.substring(0,pos);
   x=str.substring(pos+1);
   if (isInteger(s)){return isInteger(x);} 
  }
  return false;
} 
boolean tSysConfig::isIP(String str){// check that the input is an IP4 class address string
  String a; // 4 substrings to define the 4 quadrants of an IP string
  String b;
  String c;
  String d;
  // check for the . character, there needs to be 3 to be a valid ip4 address
  int pos = str.indexOf(".");
  if (pos==-1){ return false;}
  a=str.substring(0,pos);
  int pos2 = str.indexOf(".", pos);
  if (pos2==-1){ return false;}
  b=str.substring(pos+1,pos2);
  pos = str.indexOf(".", pos2);
  if (pos==-1){ return false;}
  c=str.substring(pos2+1,pos);
  d=str.substring(pos+1);
  // check is the substrings are numeric, if not then its not a valid ip
  if (!isInteger(a)){ return false;}
  if (!isInteger(b)){ return false;}
  if (!isInteger(c)){ return false;}
  if (!isInteger(d)){ return false;}
  if (a.toInt()>255){ return false;}
  if (b.toInt()>255){ return false;}
  if (c.toInt()>255){ return false;}
  if (d.toInt()>255){ return false;}
  return true;
}
  // copy values from post function is overloaded and has default parameters, if min-max are equal, range is ignored otherwise range checking is used

int tSysConfig::copyval(byte &var, char* name, byte min, byte max){
 String V;
 error = "";
 if (server.hasArg(name)) {
  V=server.arg(name);
  if ((V.length()<1)|(!isInteger(V))){
    error += "var ";
    error += name;
    error += " blank\r\n";
    errorcount++;
    errorclass=NaN;
    return NaN;
  }
    var=(byte)V.toInt();
    if(max!=min){
    if ((var<=max)&&(var>=min)){
     errorclass = OK;
     return OK;
     }
     error += "var ";
     error += name;
     error += " out of range\r\n";
     errorcount++;
     errorclass = Range;
     return Range;
   }
  return OK; } 
  else{
  error += "var ";
  error += name;
  error += " not found\r\n";
  errorcount++;
  errorclass = NotFound;
  return NotFound;
 }
}

int tSysConfig::copyval(int &var, char* name, int min, int max){
 String V;
 error = "";
 if (server.hasArg(name)) {
  V=server.arg(name);
  if ((V.length()<1)|(!isInteger(V))){
    error += "var ";
    error += name;
    error += " NaN\r\n";
    errorcount++;
    errorclass=NaN;
    return NaN;
  }
    var=V.toInt();
    if(max!=min){
    if ((var<=max)&&(var>=min)){
     errorclass = OK;
     return OK;
     }
     error += "var ";
     error += name;
     error += " out of range\r\n";
     errorcount++;
     errorclass = Range;
     return Range;
   }
   return OK;
  } 
  else{
  error += "var ";
  error += name;
  error += " not found\r\n";
  errorcount++;
  errorclass = NotFound;
  return NotFound;
 }
}
int tSysConfig::copyval(float &var, char* name, float min, float max){
 String V;
 error = "";
 if (server.hasArg(name)) {
  V=server.arg(name);
  if ((V.length()<1)|(!isFloat(V))){
    error += "var ";
    error += name;
    error += " blank\r\n";
    errorcount++;
    errorclass=NaN;
    return NaN;
  }
    var=V.toFloat();
    if(max!=min){
    if ((var<=max)&&(var>=min)){
     errorclass = OK;
     return OK;
     }
     error += "var ";
     error += name;
     error += " out of range\r\n";
     errorcount++;
     errorclass = Range;
     return Range;
   }
   return OK;
  } 
  else{
  error += "var ";
  error += name;
  error += " not found\r\n";
  errorcount++;
  errorclass = NotFound;
  return NotFound;
 }
}

int tSysConfig::copyval(time_t &var, char* name){
 String V;
 error = "";
 if (server.hasArg(name)) {
  V=server.arg(name);/*
  if ((V.length()<1)|(!isTime(V))){
    error += "var ";
    error += name;
    error += " NaN\r\n";
    errorcount++;
    errorclass=NaN;
    return NaN;
  }*/
  /*  var=V.toInt();
    if(max!=min){
    if ((var<=max)&&(var>=min)){
     errorclass = OK;
     return OK;
     }
     error += "var ";
     error += name;
     error += " out of range\r\n";
     errorcount++;
     errorclass = Range;
     return Range;
   }
   return OK;*/
  } 
  else{
  error += "var ";
  error += name;
  error += " not found\r\n";
  errorcount++;
  errorclass = NotFound;
  return NotFound;
  }
}

int tSysConfig::copyIP(IPAddress &var, char* name){
 String V;
 error = "";
 if (server.hasArg(name)) {
  V=server.arg(name);
  if ((V.length()<1)|(!isTime(V))){
    error += "var ";
    error += name;
    error += " NaN\r\n";
    errorcount++;
    errorclass=NaN;
    return NaN;
   }
    if ( var.fromString(V) ){
    return OK;
    } 
  }
  else {
  error += "var ";
  error += name;
  error += " not found\r\n";
  errorcount++;
  errorclass = NotFound;
  return NotFound;
  }
}
void tSysConfig::scanWiFi(){
  bool isConnected=WiFi.status() == WL_CONNECTED;
  int retries=0;
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  // scan for available networks
  n = WiFi.scanNetworks(); 
  for (int i = 0; i < n; i++)
  {
    WiFi.getNetworkInfo(i, ssid, encryptionType, RSSI, BSSID, channel, isHidden);
    Serial.printf("%d: %s, Ch:%d (%ddBm) %s %s\r\n", i + 1, ssid.c_str(), channel, RSSI, encryptionType == ENC_TYPE_NONE ? "open" : "", isHidden ? "hidden" : "");
  }
  if (isConnected){  
    WiFi.begin(_data.AccessPoints[0].WiFiname, _data.AccessPoints[0].WiFipassword); 
    // wait for connection here
    while ((WiFi.status() != WL_CONNECTED) && (retries<10)) {
    delay(1500);
    Serial.print(".");
    ++retries;
  } 
 }
}
/*int tSysConfig::copyval(bool &var, char* name); // checkbox is unlike other fields, if its checked, it is included otherwise the field is not returned at all by the form post

int tSysConfig::copyval(char* var, char* name, int size, int min);*/

tSysConfig sysConfig;
