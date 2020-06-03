/*  author: Peter Dunne, +44 7932 676 847 peterjazenga@gmail.com
 *  purpose: to control a relay or SSR via radio signal or WiFi signal
 *  requirements: serial interface for radio, read only mode, initial design uses 433 mhz receiver
 *  WiFi interface to facilitate remote control using JSON packets
 *  Web interface to Configure WiFi interface and define control parameters for the radio transmitter device
 *  Bluetooth (ESP32 only) control and Configuration interface to phones/tablets/etc.
 *  
 *  
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
byte APConfig; 
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

// main classes for Configuration management, time management and NAT
enum errorclass {ecRange,ecRequired,ecNaN,ecPassword};
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
 protected: 
  String error; // used singularly during form processing, each new element resets this
  int errorclass; // used singularly during form processing, each new element resets this
  int errorcount;
  String HTML; // used to build the web page
  String options; // used to build options lists during processing, this is necessary so that the error class for string lists can be defined during post processing

 public:
   int configPin = D2;
 public:
// this is API to the Config system but the only routines that the user is required to utilize are init(); & run();
// if desired, the device name can be changed prior to calling init
  ESP8266WebServer server;
  ESP8266WiFiMulti WiFiMulti;
  void firstrun(); 
  /* this is meant to engage an automated Configuration mode that connects to a predefined WiFi and then calls some functions to get the logo, CSS, about, help and initConfig files 
  *  firstrun passes the device Mac number to the called program, this then sets a unique login ID for the device Configuration file  
  *  firstrun also exposes the AP with the default device name in the format of TRL-xxxxxx with the password of TRLinitialize
  */ 
  void ntpsetup(); // time system
  void natsetup(); // NAT relay functions
  void websetup(); // sets up the network and servers
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
  bool form(char* name, char* caption, char* hint, String _url);// all forms are post request only
  bool _form(String buttoncaption=""); // if the buttoncaption is not included, then the standard send button is created
  
  void fieldset(String name); // use to create groupboxes on the form
  void _fieldset();

  bool edit(char* name, char* caption, char* hint, char* data, int size, int min=0, int max=0);
  bool edit(char* name, char* caption, char* hint, byte &data, byte min=0, byte max=0);
  bool edit(char* name, char* caption, char* hint, int &data, int min=0, int max=0);
  bool edit(char* name, char* caption, char* hint, float &data, float min=0.0, float max=0.0);
  bool edit(char* name, char* caption, char* hint, time_t &data, time_t min=0, time_t max=0);
  // unlike other fields, password auto includes Label & verify dialog
  bool password(char* name, char* hint, char* data, int size, int min=0, int max=0);
  bool text(char* name, char* caption, char* hint, char* data, int size, int min=0, int max=0);

  bool droplist(char* name, char* caption, char* hint, int &data);
  bool droplist(char* name, char* caption, char* hint, char* data, int size);// using this forces options to use the name field as value
  bool optiongroup(char* name);
  bool option(char* name, String value="");
  bool _droplist(); // ends the droplist, completes post verification and does the error message
  bool checkbox(char* name, char* caption, char* hint, bool &data);
  bool checkbox(char* name, char* caption, char* hint, int &data);
  bool checkbox(char* name, char* caption, char* hint, char* data);
  bool radio(char* name, char* caption, char* hint, int &data);

  // additional html elements of significant use to us
  // https://www.w3schools.com/tags/tag_meter.asp
  void meter(String label, int value, int min=0, int max=0);// name is not required as this item does not return information
  void meter(String label, float value, float=0, float=0);// name is not required as this item does not return information
  // https://www.w3schools.com/tags/tag_progress.asp
  void progress(String label, int value, int min=0, int max=0);// name is not required as this item does not return information
  void progress(String label, float value, float=0, float=0);// name is not required as this item does not return information
  // https://www.w3schools.com/tags/tag_details.asp
  void details(String label, String text);
  void newURL(char* url, String target="");// simply adds a URL to the page
  void menuItem(char* url, String target="");// adds URL as a button item using w3c.css
  void newURL(char* url, String target="", String script="");// simply adds a URL to the page
  void menuItem(char* url, String target="", String script="");// adds URL as a button item using w3c.css

  void Config();
  void Sensors();
  void Timers();
  void UserID();
  void ACL(); 
  void getLogo();
  void getCSS();
  void getCharts();// does the charting page
  void drawGraph();// draws the actual image 
  void getAbout();
  void getHelp();
  void getFooter();// used by every page
  void getHeader();// used by every page
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
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  FlashMode_t ideMode = ESP.getFlashChipMode();
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
  websetup();
  natsetup();
  ntpsetup();
  // OTA update services
  
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

void tSysConfig::ntpsetup() {
  
}

void tSysConfig::natsetup() {
  if (_data.APConfig==3){
  // give DNS servers to AP side
  dhcps_set_dns(0, WiFi.dnsIP(0));
  dhcps_set_dns(1, WiFi.dnsIP(1));

  WiFi.softAPConfig(  // enable AP, with android-compatible google domain
    IPAddress(172, 217, 28, 254),
    IPAddress(172, 217, 28, 254),
    IPAddress(255, 255, 255, 0));
  WiFi.softAP(_data.APname, _data.APpassword, _data.APchannel);
  err_t ret = ip_napt_init(NAPT, NAPT_PORT);
  if (ret == ERR_OK) {
    ret = ip_napt_enable_no(SOFTAP_IF, 1);
    if (ret == ERR_OK) {
    }
  }
  }
}
void tSysConfig::websetup() {

  server.on("/", [&]{ indexPage(); });
  server.on("/Config", [&]{ Config(); });
  server.on("/timers", [&]{ Timers(); });
  server.on("/userid", [&]{ UserID(); });
  server.on("/sensors", [&]{ Sensors(); });
  server.on("/acl", [&]{ ACL(); });

  server.on("/logo.jpg", [&]{ getLogo();} );
  server.on("/style.css", [&]{ getCSS();} );
  server.on("/charts", [&]{ getCharts();} );
  server.on("/graph", [&]{ drawGraph();} );
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
    if (!ConfigFile) {return false;}
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
    _data.APConfig=1; // softap is always available
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
    firstrun();
}  
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
  
}
void tSysConfig::Config(){ 
  
}
void tSysConfig::Timers(){ 
  
}
void tSysConfig::UserID(){ 
  
}
void tSysConfig::ACL(){ 
  
}
void tSysConfig::getLogo(){
  
}
void tSysConfig::getCSS(){
  
}
void tSysConfig::getCharts(){
  
}
void tSysConfig::drawGraph(){ 
  
}
void tSysConfig::getAbout(){
    
  }
void tSysConfig::getHelp(){
    
  }
void tSysConfig::Sensors(){
    
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
  }
// form creation support
bool tSysConfig::form(char* name, char* caption, char* hint, String _url){
   // all forms are post request only
  }
bool tSysConfig::_form(String buttoncaption){
  // if the buttoncaption is not included, then the standard send button is created
  }
void tSysConfig::fieldset(String name){
  // use to create groupboxes on the form
}
void tSysConfig::_fieldset(){
    
  }
bool tSysConfig::edit(char* name, char* caption, char* hint, char* data, int size, int min, int max){
    
  }
bool tSysConfig::edit(char* name, char* caption, char* hint, byte &data, byte min, byte max){
    
  }
bool tSysConfig::edit(char* name, char* caption, char* hint, int &data, int min, int max){
    
  }
bool tSysConfig::edit(char* name, char* caption, char* hint, float &data, float min, float max){
    
  }
bool tSysConfig::edit(char* name, char* caption, char* hint, time_t &data, time_t min, time_t max){
    
  }
bool tSysConfig::password(char* name, char* hint, char* data, int size, int min, int max){
  // unlike other fields, password auto includes Label & verify dialog  
}
bool tSysConfig::text(char* name, char* caption, char* hint, char* data, int size, int min, int max){
    
  }
bool tSysConfig::droplist(char* name, char* caption, char* hint, int &data){
    
  }
bool tSysConfig::droplist(char* name, char* caption, char* hint, char* data, int size){
    // using this forces options to use the name field as value
  }
bool tSysConfig::optiongroup(char* name){
    
  }
bool tSysConfig::option(char* name, String value){
    
  }
bool tSysConfig::_droplist(){
    // ends the droplist, completes post verification and does the error message
  }
bool tSysConfig::checkbox(char* name, char* caption, char* hint, bool &data){
    
  }
bool tSysConfig::checkbox(char* name, char* caption, char* hint, int &data){
    
  }
bool tSysConfig::checkbox(char* name, char* caption, char* hint, char* data){
    
  }
bool tSysConfig::radio(char* name, char* caption, char* hint, int &data){
    
  }
  // additional html elements of significant use to us
void tSysConfig::meter(String label, int value, int min, int max){
    // name is not required as this item does not return information
  }
void tSysConfig::meter(String label, float value, float, float){
    // name is not required as this item does not return information
  }
void tSysConfig::progress(String label, int value, int min, int max){
    // name is not required as this item does not return information
  }
void tSysConfig::progress(String label, float value, float, float){
    // name is not required as this item does not return information
  }
void tSysConfig::details(String label, String text){
    
  }
void tSysConfig::newURL(char* url, String target){
    // simply adds a URL to the page
  }
void tSysConfig::menuItem(char* url, String target){
    // adds URL as a button item using w3c.css
  }
void tSysConfig::newURL(char* url, String target, String script){
    // simply adds a URL to the page
  }
void tSysConfig::menuItem(char* url, String target, String script){
  // adds URL as a button item using w3c.css
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
/*int tSysConfig::copyval(bool &var, char* name); // checkbox is unlike other fields, if its checked, it is included otherwise the field is not returned at all by the form post

int tSysConfig::copyval(char* var, char* name, int size, int min);*/

tSysConfig sysConfig;
