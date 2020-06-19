/* author: Peter Dunne, +44 7932 676 847 peterjazenga@gmail.com
 * purpose: to provide an effective configuration portal and the basis of ESP32 & ESP8266 IOT projects
 * Bluetooth (ESP32 only) control and Configuration interface to phones/tablets/etc.
 * the CSS file used is w3.css as it is compact and free to use
 * the js file w3.js provides enough functions to enable compact devices like the ESP32 & ESP8266 with reasonable web interfaces
 * https://www.w3schools.com/w3js/default.asp
 * https://www.w3schools.com/w3css/default.asp 
 * https://www.w3schools.com/html/default.asp
 * the author is not affiliated with w3schools however its a clear source of useful information on html, css and several programming languages
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <FS.h>   //Include File System Headers
#include <time.h>  // time() ctime()
#include <sys/time.h> // struct timeval
#include <TimeLib.h>
#include <coredecls.h> // settimeofday_cb()
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <dhcpserver.h>
#include <sntp.h>      // sntp_servermode_dhcp()

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
 byte dow;// used as a set of flags to define what day of the week its active
 // the msb is the enable bit for the timer
} ttimer;

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
char NTPserver[128];
float setPoint; // also used as setPoint for PID loops
float rangeValue;
time_t min_on;
time_t max_on;
time_t min_off;
byte OTA;
char OTApass[32];
float iceGuard; //used by heating and cooling to prevent icing, must be >0 to be active
bool activeHigh; // heating activates when temperature drops below the lower trigger value
bool PIDcontrolled; // if running on a PID, the value used for setPoint is setPoint
ttimer timers[32]; // all timers default to inactive and zero values 
short burncount;
time_t burntime;
char userName[32];
char userPass[32];
char userEmail[128];
char firstname[32];
char lastname[32];
char phone[32];
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
#define NoString NaN-1
#define Range NoString-1
#define RangeLow Range-1
#define RangeHigh RangeLow-1
#define NotFound RangeHigh-1
#define InvalidTime NotFound-1
#define BadAddress InvalidTime-1
#define OK 0

#define FAST_BLINK 50 // use for errors
#define SLOW_BLINK 150 // use for standard operations
#define PT_IDLE  0b10100000101000001010000010100000
#define PT_UPLOADING 0b10101010101010101010101010101010
#define PT_CONNECTED 0b10001000100010001000100010001000
#define PT_SCANNING 0b11010000110100001101000011010000
#define SCAN_PERIOD 5000
#define CONNECT_PERIOD 500

// global variables
IPAddress ip(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);
byte APchannel = 11;
unsigned int localPort = 2390;  // local port to listen for UDP packets

// main classes for Configuration management, time management and NAT
enum s_class {s_none, s_byte, s_int32_t, s_float, s_text};// used by the select functions
enum eSensorClass {s_undefined, s_NTC, s_BMP, s_BME, s_ADC, s_Freq, s_PWM, s_Weight};

typedef std::function<void(time_t trigger)> TTimerFunction;
 
class tSysConfig : ESP8266WebServer {
 private:
  dataframe _data; // the data is contained in the private section as its not meant to be directly interatecd with by the main program
  dataframe _formdata; // the data contained herein is used to process the form data
 // LED information for heartbeat LED
  long lastBlinkMillis;
  int32_t blinkstate=0;// defines which blink is used, 0-3 are slow 4-7 are fast blink
  int32_t LEDstate=0;//inidicates which bit of the pattern is in use
 // WiFi connection information
  long currentMillis = 0;
  long lastScanMillis;
  long lastConnectMillis;
 // network scanning info
  bool STAconnected=false;
  int32_t n=-2;
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
  int32_t error; // used singularly during form processing, each new element resets this
  int32_t errorcount;
  String HTML; // used to build the web page
  char buffer[256];
  bool inForm;
  bool inFieldset;
  s_class inSelect; // if we are in a select group we need to know the class so it processes the correct information
  bool inOptgroup;
  bool lightHTML;
  public:
  int32_t configPin = D2;
  char *author;
  char *copyright;
  char *version;
  int32_t charsize;
  int32_t major;
  int32_t minor;
  bool hasBluetooth=false;
  // the sensor classes are for future use
  eSensorClass SensorClass=s_undefined;
  String SensorName;
  int SensorFrequency=100; // time in milliseconds
 public:
// this is API to the Config system but the only routines that the user is required to utilize are tSysConfig(); & run();
// if desired, the device name can be changed prior to calling init
 htmlproperties webobj;
 ESP8266WebServer server;
 ESP8266WiFiMulti WiFiMulti;
 WiFiUDP udp; 
 /* this is meant to engage an automated Configuration mode that connects to a predefined WiFi and then calls some functions to get the logo, CSS, about, help and initConfig files 
 * firstrun passes the device Mac number to the called program, this then sets a unique login ID for the device Configuration file 
 * firstrun also exposes the AP with the default device name in the format of TRL-xxxxxx with the password of TRLinitialize
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
 void blink(); 
 void init(); // startup code
 void run(); // called from the loop code to maintain periodic functions
 // there are lots of web functions as this is a web based Configuration program
 void indexPage(void);
 // procedures to handle forms
 boolean isinteger(String str); 
 boolean isFloat(String str);
 boolean isTime(String str);
 boolean isIP(String str);

 // copy values from post function is overloaded and has default parameters, if min-max are equal, range is ignored otherwise range checking is used
 int32_t copyval(byte &var, char const *name, byte min=0, byte max=0);
 int32_t copyval(int32_t &var, char const *name, int32_t min=0, int32_t max=0);
 int32_t copyval(float &var, char const *name, float min=0, float max=0);
 int32_t copyval(char* var, char const *name, int32_t size, int32_t min=0);
 int32_t copyval(time_t &var, char const *name);
 int32_t copyval(bool &var, char const *name); // checkbox is unlike other fields, if its checked, it is included otherwise the field is not returned at all by the form post
 int32_t copyIP(IPAddress &var, char const *name);

 // form creation support
 bool form(htmlproperties obj);// all forms are post request only
 bool form(); // if the buttoncaption is not included, then the standard send button is created
 void tab(char* tag);
 void fieldset(char* tag); // use to create groupboxes on the form
 void fieldset();
 void label(htmlproperties obj);
 bool edit(htmlproperties obj, char* data, int32_t size);
 bool editurl(htmlproperties obj, char* data, int32_t size);
 bool editemail(htmlproperties obj, char* data, int32_t size);
 bool edittel(htmlproperties obj, char* data, int32_t size);
 bool editTime(htmlproperties obj, time_t &data);
 bool edit(htmlproperties obj, byte &data, byte min=0, byte max=0);
 bool edit(htmlproperties obj, int32_t &data, int32_t min=0, int32_t max=0);
 bool edit(htmlproperties obj, float &data, float min=0.0, float max=0.0);
 bool edit(htmlproperties obj, time_t &data, time_t min=0, time_t max=0);
 // unlike other fields, password auto includes Label & verify dialog
 bool password(htmlproperties obj, char* data, int32_t size, int32_t min=0, int32_t max=0);
 bool text(htmlproperties obj, char* data, int32_t size, int32_t min=0, int32_t max=0);

 bool selectList(htmlproperties obj,int32_t &data);
 bool selectbyte(htmlproperties obj,byte &data);
 bool selectList(htmlproperties obj,float &data); 
 bool selectList(htmlproperties obj,char* data, int32_t size);// using this forces options to use the name field as value
 bool selectList();// terminates the list
 bool optiongroup(char const *name); //groups a value list
 bool optiongroup(); //terminates the group

 bool option(bool data, char *name);
 bool option(byte data, char *name);
 bool option(int32_t data, char *name);
 bool option(float data, char *name);
 bool option(char* data, char *name);
 bool checkbox(htmlproperties obj, bool &data);
 bool checkbox(htmlproperties obj, int32_t &data);
 bool checkbox(htmlproperties obj, char* data, int32_t size);
 bool radio(htmlproperties obj, int32_t &data);
 bool radio(htmlproperties obj, char* data, int32_t size);
 bool editrange(htmlproperties obj, int32_t &data, int32_t min=0, int32_t max=0);
 // additional html elements of significant use to us
 // https://www.w3schools.com/tags/tag_meter.asp
 void meter(htmlproperties obj, int32_t value, int32_t min=0, int32_t max=0);
 void meter(htmlproperties obj, float value, float min =0, float max =0);
 // https://www.w3schools.com/tags/tag_progress.asp
 void progress(htmlproperties obj, int32_t value, int32_t min=0, int32_t max=0);
 void progress(htmlproperties obj, float value, float min =0, float max =0);
 // https://www.w3schools.com/tags/tag_details.asp
 void details(htmlproperties obj);
 void div();
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
 void getSCjs();
 bool webAuth(); // used by restricted Configuration pages
 void handleNotFound();
 // to support a Configuration application, we have the JSON data exchanges, there are no individual pages in the JSON processing
 void JSON();// format bytes
 // additional utility routines
 String formatbytes(size_t bytes);
};

void tSysConfig::init() {
 WiFi.persistent(false);
 pinMode(LED_BUILTIN, OUTPUT);
 realSize = ESP.getFlashChipRealSize();
 ideSize = ESP.getFlashChipSize();
 ideMode = ESP.getFlashChipMode();
 Serial.printf(" ESP8266 Chip id = %08X\n\r", ESP.getChipId());
 Serial.printf("Flash real id: %08X\n\r", ESP.getFlashChipId());
 Serial.printf("Flash real size: %u bytes\n\r", realSize);
 Serial.printf("Flash ide size: %u bytes\n\r", ideSize);
 Serial.printf("Flash ide speed: %u Hz\n\r", ESP.getFlashChipSpeed());
 Serial.printf("Flash ide mode: %s\n\r", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
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
 case 0: ArduinoOTA.handle(); break;
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
 server.on("/timer.html", [&]{ Timers(); });
 server.on("/acl.html", [&]{ ACL(); });
 server.on("/register.html", [&]{ Register(); });
 server.on("/mqtt.html", [&]{ MQTT(); });
 server.on("/sysinfo.html", [&]{ Sysinfo(); });
 server.on("/help.html", [&]{ getHelp(); });
 server.on("/about.html", [&]{ getAbout(); });
 server.on("/favicon.ico", [&] { getIcon();} );
 server.on("/logo.png", [&]{ getLogo();} );
 server.on("/w3.css", [&]{ getCSS();} );
 server.on("/w3.js", [&]{ getJS();} );
 server.on("/sc.js", [&]{ getSCjs();} );
 server.onNotFound([&](){ handleNotFound(); });
 server.begin();
}

bool tSysConfig::readConfig(){
// read the parameters from flash
 Serial.println("Opening SPIFFS /config.bin");
 File ConfigFile = SPIFFS.open("/config.bin", "r");
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
 if(!SPIFFS.begin())
 {
 Serial.println("SPIFFS Initialization...failed");
 } else { 
 Dir dir = SPIFFS.openDir("/");
 while (dir.next()) {
  String fileName = dir.fileName();
  size_t fileSize = dir.fileSize();
  Serial.printf("FS File: %s, size: %s\r\n", fileName.c_str(), formatbytes(fileSize).c_str());
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
} 
}
void tSysConfig::initWiFi(){
 byte retries = 0;
 WiFi.mode(WIFI_AP_STA);
 WiFi.disconnect();
 // scan for available networks
 scanWiFi();
 for ( byte i = 0; i < 5; i++ ) {
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
 // Turn on local Access point
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
 Serial.println("config.bin written");
 return readConfig();
}

void tSysConfig::updateConfig() {
 // intended to reinitialize the ESP without rebooting
}

void tSysConfig::blink() {
 int32_t t=SLOW_BLINK;
if (blinkstate>3) {t=FAST_BLINK;}
 if (currentMillis - lastBlinkMillis > t)
 {
 LEDstate++;
 if (LEDstate>31){LEDstate=0;}
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
 ArduinoOTA.begin(true); 
 }
 
void tSysConfig::indexPage(void) {
  File dataFile = SPIFFS.open("/index.html", "r"); 
 if (dataFile.size()<=0) {Serial.println("bad file size");
 }
 if (server.streamFile(dataFile, "text/html") != dataFile.size()) {Serial.println(F("index.html streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::Config(){ 
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Config Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\">");
 HTML+=F("<script type=\"text/javascript\" src=\"sc.js\"></script>");
 HTML+=F("</head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
 HTML+=F("System configuration <button onclick=\"openTab('A')\">WiFi connection</button>");
 HTML+=F("<button onclick=\"openTab('B')\">AP</button>");
 if (hasBluetooth){ HTML+=F("<button onclick=\"openTab('C')\">Bluetooth</button>");}
 HTML+=F("<button onclick=\"openTab('D')\">Time</button><button onclick=\"openTab('E')\">Updates</button></div>");
 
 webobj.css=F("w3-container w3-card-4 w3-light-grey w3-text-blue w3-margin");
 webobj.value="";
 webobj.placeholder="";
 webobj.label="";
 webobj.name="";
 webobj.required=false;
 webobj.inlineJS="";
 webobj.functionalJS="";
 webobj.hint="";
 form(webobj);

 fieldset("A");
 webobj.name="WiFiname_0";
 webobj.label="WiFi 1:";
 webobj.placeholder="WiFi SSID";
 edit(webobj,_data.AccessPoints[0].WiFiname,32);
 webobj.name="WiFipass_0";
 webobj.label="Password:";
 password(webobj,_data.AccessPoints[0].WiFipassword,32);
 webobj.name="WiFiname_1";
 webobj.label="WiFi 2:";
 edit(webobj,_data.AccessPoints[1].WiFiname,32);
 webobj.name="WiFipass_1";
 webobj.label="Password:";
 password(webobj,_data.AccessPoints[1].WiFipassword,32);
 webobj.name="WiFiname_2";
 webobj.label="WiFi 3:";
 edit(webobj,_data.AccessPoints[2].WiFiname,32);
 webobj.name="WiFipass_2";
 webobj.label="Password:";
 password(webobj,_data.AccessPoints[2].WiFipassword,32);
 webobj.name="WiFiname_3";
 webobj.label="WiFi 4:";
 edit(webobj,_data.AccessPoints[3].WiFiname,32);
 webobj.name="WiFipass_3";
 webobj.label="Password:";
 password(webobj,_data.AccessPoints[3].WiFipassword,32);
 webobj.name="WiFiname_4";
 webobj.label="WiFi 5:";
 edit(webobj,_data.AccessPoints[4].WiFiname,32);
 webobj.name="WiFipass_4";
 webobj.label="Password:";
 password(webobj,_data.AccessPoints[4].WiFipassword,32); 

 fieldset("B");
 webobj.name=F("APname");
 webobj.label=F("AP name:");
 webobj.placeholder=F("Access point name");
 edit(webobj,_data.APname,32);
 webobj.name=F("APpass");
 webobj.label=F("Password:");
 password(webobj,_data.APpassword,32);
 webobj.name=F("APchannel");
 webobj.label=F("AP channel:");
 webobj.placeholder=F("range is 1 - 13");
 edit(webobj,_data.APchannel,1,13);
 webobj.name=F("APconfig");
 webobj.label=F("Ap Configuration");
 webobj.placeholder="";
 // the fun part is to put known networks at the top of the list, that means searching through and comparing to those stored in memory
 selectbyte(webobj,_data.APconfig);
 option(0,"Disabled");
 option(1, "Active on boot");
 option(2, "Always active");
 option(3, "Active Android compatible");
 selectList();

 if (hasBluetooth){
 fieldset("C");
 webobj.name=F("BLEname");
 webobj.label=F("BLE name:");
 webobj.placeholder=F("Bluetooth name (your device name)");
 edit(webobj,_data.BLEname,32);
 webobj.name=F("BLEpassword");
 webobj.label=F("BLE password:");
 password(webobj,_data.BLEpassword,32);
 webobj.name=F("BLEconfig");
 webobj.label=F("BLE configration:");
 selectbyte(webobj,_data.BLEConfig);
 option(-1,"Disabled");
 option(0, "Sensors only");
 option(1, "All");
 selectList();
 }
 
 fieldset("D");
 webobj.name=F("NTPserverValue");
 webobj.label=F("NTP server:");
 webobj.placeholder=F("must be a valid time server such as north-america.pool.ntp.org");
 edit(webobj,_data.NTPserver,128);
 webobj.name=F("TZ");
 webobj.label=F("GMT offset");
 webobj.placeholder=F("value in minutes");
 edit(webobj,_data.TZ);

 fieldset("E"); 
 webobj.name="OTA";
 webobj.label="OTA configuration:";
 selectbyte(webobj,_data.OTA);
 option(0,"Disabled");
 option(1, "5 minutes");
 option(2, "Always on");
 selectList(); webobj.name="OTApass";
 webobj.label="OTA Password:";
 password(webobj,_data.OTApass,32);

 fieldset();
 form();
   if ( server.method() == HTTP_POST ){ writeConfig();}
 HTML+=F("</body></html>");
 server.send(200, "text/html", HTML);
 HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
}
void tSysConfig::Sensors(){
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Sensor Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
 HTML+=F("Sensor configuration</div>");
 webobj.name="sensors";
 form(webobj);
 
 form();
    if ( server.method() == HTTP_POST ){ writeConfig();}
HTML+=F("</body></html>");
 server.send(200, "text/html", HTML);
 HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
}
void tSysConfig::Timers(){ 
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">Timers</div>");
 webobj.name="timers";
 form(webobj); 
 HTML+=F("<table id = \"tt\"></table><script>var time, i, id, x = \"\";\r\n var time ='{\"alarms\":[");
 
/* add the variables here for the form, deliberately ignore blank entries on the first pass and include them as an addendum so that 
 *  all active timers go to the top of the page
 *  use a variable to tell the script how many placeholders to enter for the disabled/blank alarms
*/
 tmElements_t stime,etime; 
 char name[16];
 for(int i = 0; i<16 ;i++) 
 { 
  if ( server.method() == HTTP_POST ){
   sprintf(name,"st_%d",i);
   error=copyval(_data.timers[i].startTime,name);
   sprintf(name,"et_%d",i);
   error=copyval(_data.timers[i].endTime,name);
   sprintf(name,"d_%d",i);
   error=copyval(_data.timers[i].dow,name);
 }
  
 // if (_data.timers[i].startTime!=_data.timers[i].endTime) 
  {
   // split the time variable
  breakTime(_data.timers[i].startTime, stime); 
  breakTime(_data.timers[i].endTime, etime); 
  if (i) {
   sprintf(buffer,",[\"%02d:%02d\",\"%02d:%02d\",%d]",stime.Hour, stime.Minute, etime.Hour, etime.Minute, _data.timers[i].dow); } else {
   sprintf(buffer,"[\"%02d:%02d\",\"%02d:%02d\",%d]",stime.Hour, stime.Minute, etime.Hour, etime.Minute, _data.timers[i].dow);         
   }
   HTML+=buffer;
  }// else {++dis;}// dont add a variable, instead we give a count in the json object to let the script know how many blank fields to add
 }

  sprintf(buffer,"]}';\r\n");
// dynamically create the form 
 HTML+=buffer;
 HTML+=F("var times = JSON.parse(time);\r\n  id=0;\r\n  for (i in times.alarms) {\r\n ++id; x+=\"<tr><td>\"+ id +\"</td><td><input type=\\\"time\\\" name=\\\"st_\"+i+");
 HTML+=F("\"\\\" value=\\\"\" + times.alarms[i][0] + \"\\\"></td><td><input type=\\\"time\\\" name=\\\"et_\"+i+\"\\\" value=\\\"\" + times.alarms[i][1] +");  
 HTML+=F("\"\\\"></td><td><select name=\\\"d_\"+i+\"\\\" value=\\\"\" + times.alarms[i][2]+");
 HTML+=F("\"\\\"><option value=0>Disabled</option><option value=1>Sun</option><option value=2>Mon</option><option value=3>Tue</option><option value=4>Wed</option><option value=5>Thu</option><option value=6>Fri</option><option value=7>Sat</option><option value=8>Daily</option><option value=9>Once</option></select></td></tr>\";");
 HTML+=F("}\r\n  document.getElementById(\"tt\").innerHTML = \"<TR><TD>ID</td><td>Start</td><td>Stop</td><td><abbr title=\\\"Day of week\\\">DOW<abbr></TD></tr>\"+x; </script>");

 form();
   if ( server.method() == HTTP_POST ){ writeConfig();}
 HTML+=F("</body></html>"); 
 server.send(200, "text/html", HTML);
 HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
}

void tSysConfig::ACL(){ 
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
 HTML+=F("Access control</div>");
 webobj.name="ACL";
 form(webobj);
 webobj.name="fname";
 webobj.label="&#128100";
 webobj.placeholder="Username Name";
 edit(webobj,_data.userName,32);
 webobj.name="lname";
 webobj.label="&#128272";
 webobj.placeholder="Password";
 password(webobj,_data.userPass,32);
 
 form();
   if ( server.method() == HTTP_POST ){ writeConfig();}
 HTML+=F("</body></html>"); 
 server.send(200, "text/html", HTML);HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
}
void tSysConfig::Register(){ 
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
 HTML+=F("Device registration</div>");
 webobj.name="register";
 form(webobj);
 webobj.name="fname";
 webobj.label="&#128100";
 webobj.placeholder="First Name";
 edit(webobj,_data.firstname,32);
 webobj.name="lname";
 webobj.label="&#128101";
 webobj.placeholder="Last Name";
 edit(webobj,_data.lastname,32);
 webobj.name="email";
 webobj.label="&#9993";
 webobj.placeholder="Email address";
 editemail(webobj,_data.userEmail,128);
 webobj.name="phone";
 webobj.label="&#9742";
 webobj.placeholder="Phone number including IDD";
 edittel(webobj,_data.phone,32);
 webobj.name="devname";
 webobj.label="&#128397";
 webobj.placeholder="Device Name";
 edit(webobj,_data.NodeName,32);
 form();
    if ( server.method() == HTTP_POST ){ writeConfig();}
HTML+=F("</body></html>"); 
 server.send(200, "text/html", HTML);HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
}
void tSysConfig::MQTT(){ 
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
 HTML+=F("MQTT configuration</div>");
 webobj.name="MQTT";
 form(webobj);
 
 form();
 HTML+=F("</body></html>"); 
 server.send(200, "text/html", HTML);HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
}

void tSysConfig::Sysinfo(){ 
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
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
 if (ideSize != realSize) { HTML+=F("<td class=\"w3-red\">wrong!.</td></tr>");
 } else {
 HTML+=F("<td>ok.</td></tr>");
 } 
 sprintf(buffer," <tr><td>Connected to</td><td>%s</td></tr>",WiFi.SSID().c_str());
 HTML+=buffer;
 IPAddress IP = WiFi.localIP();
 IPAddress GW = WiFi.gatewayIP();
 char buf[50];
 sprintf(buf, "%d.%d.%d.%d<br><a href=\"http://%d.%d.%d.%d\" target=\"_blank\">%d.%d.%d.%d</a>", IP[0],IP[1],IP[2],IP[3], GW[0],GW[1],GW[2],GW[3], GW[0],GW[1],GW[2],GW[3] );
 sprintf(buffer,"<tr><td>IP address<BR>Gateway</td><td>%s</td></tr>",buf);
 HTML+=buffer;
 sprintf(buffer," <tr><td>Access point name</td><td>%s</td></tr>",_data.APname);
 HTML+=buffer;
 IP = WiFi.softAPIP();
 sprintf(buf, "%d.%d.%d.%d", IP[0],IP[1],IP[2],IP[3] );
 sprintf(buffer,"<tr><td>IP address</td><td>%s</td></tr>",buf);
 HTML+=buffer;

 sprintf(buffer," <tr><td>Time server</td><td>%s</td></tr>",_data.NTPserver);
 HTML+=buffer;
 sprintf(buffer," <tr><td>Compilation date</td><td>%s @ %s</td></tr>",__DATE__,__TIME__);
 HTML+=buffer;
 
   if ( server.method() == HTTP_POST ){ writeConfig();}
 HTML+=F("</body></html>");
 server.send(200, "text/html", HTML);HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder=""; 
}

void tSysConfig::getIcon(){
 File dataFile = SPIFFS.open("/favicon.ico", "r"); 
 if (dataFile.size()<=0) {Serial.println("icon bad file size");
 }
 if (server.streamFile(dataFile, "image/vnd") != dataFile.size()) {Serial.println(F("icon streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::getLogo(){
 File dataFile = SPIFFS.open("/logo.png", "r"); 
 if (dataFile.size()<=0) {Serial.println("logo bad file size");
 }
 if (server.streamFile(dataFile, "image/png") != dataFile.size()) {Serial.println(F("Logo streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::getCSS(){
 File dataFile = SPIFFS.open("/w3.css", "r"); 
 if (dataFile.size()<=0) {Serial.println("w3.css bad file size");
 }
 if (server.streamFile(dataFile, "text/css") != dataFile.size()) {Serial.println(F("CSS streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::getJS(){
 File dataFile = SPIFFS.open("/w3.js", "r");
 if (dataFile.size()<=0) {Serial.println(F("sc.js bad file size"));
 }
 if (server.streamFile(dataFile, "text/javascript") != dataFile.size()) {Serial.println(F("JS streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::getSCjs(){
 File dataFile = SPIFFS.open("/sc.js", "r"); 
 if (dataFile.size()<=0) {Serial.println(F("sc.js bad file size"));
 }
 if (server.streamFile(dataFile, "text/javascript") != dataFile.size()) {Serial.println(F("sc.js streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::getCharts(){
 HTML=F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>ESP Timers Page</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
 HTML+=F("<link rel=\"stylesheet\" type=\"text/css\" href=\"w3.css\"><script src=\"sc.js\"></script></head><body onload=\"rewrite()\"><div class=\"w3-container w3-blue\">");
 HTML+=F("Charts</div>");

 HTML+=F("</body></html>"); 
 server.send(200, "text/html", HTML);HTML=""; webobj.name="", webobj.css="";webobj.label=""; webobj.placeholder="";
 
}
void tSysConfig::drawGraph(){ 
 
}
void tSysConfig::getAbout(){
 File dataFile = SPIFFS.open("/about.html", "r"); 
 if (dataFile.size()<=0) {Serial.println(F("about.html bad file size"));
 }
 if (server.streamFile(dataFile, "text/html") != dataFile.size()) {Serial.println(F("about.html streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::getHelp(){
 File dataFile = SPIFFS.open("/help.html", "r"); 
 if (dataFile.size()<=0) {Serial.println(F("help.htm bad file size"));
 }
 if (server.streamFile(dataFile, "text/html") != dataFile.size()) {Serial.println(F("help.htm streaming error"));
 }
 dataFile.close(); 
}
void tSysConfig::handleNotFound(){
 
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
 if (inForm){form();} // ensures all previous forms are closed before we create another, nested forms are not allowed
 sprintf(buffer, "<form label=\"%s\" name=\"%s\" method=\"post\" autocomplete=\"on\">",obj.css.c_str(), obj.name.c_str());
 HTML+=buffer;
 }
bool tSysConfig::form(){
 // need to figure out how to autoinclude a submit button
 HTML+=F("<button class=\"w3-button w3-block w3-section w3-blue w3-ripple w3-padding\">Save</button></form>");
 }
void tSysConfig::fieldset(char* tag){
 // use to create groupboxes on the form
 if (inFieldset){ fieldset();}
 inFieldset=true;
 sprintf(buffer, "<fieldset id=\"%s\">",tag);
 HTML+=buffer;
}
void tSysConfig::fieldset(){
 if (inFieldset){
  inFieldset=false;
  HTML+=F("</fieldset>");
 }
 }
void tSysConfig::tab(char* tag){ 
 sprintf(buffer, "<button onclick=\"openTab('%s')\">Updates</button>",tag);
 HTML+=buffer;
}
bool tSysConfig::edit(htmlproperties obj, char* data, int32_t size){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  error=copyval(data,obj.name.c_str(),size);
 }
 sprintf(buffer,"<input type=\"text\" name=\"%s\" label=\"%s\" value=\"%s\" placeholder=\"%s\" size=\"%d\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(),size, (obj.required)?" REQUIRED ":"");
 HTML+=buffer;
}
bool tSysConfig::edit(htmlproperties obj, byte &data, byte min, byte max){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  error=copyval(data,obj.name.c_str(),min,max);
 }
 if (min!=max){
 sprintf(buffer,"<input type=\"number\" name=\"%s\" label=\"%s\" value=\"%d\" placeholder=\"%s\" min=\"%d\" max=\"%d\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(),min, max, (obj.required)?" REQUIRED ":"");
 } else {
 sprintf(buffer,"<input type=\"number\" name=\"%s\" label=\"%s\" value=\"%d\" placeholder=\"%s\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(), (obj.required)?" REQUIRED ":"");  
 }
 HTML+=buffer;
 }
bool tSysConfig::edit(htmlproperties obj, int32_t &data, int32_t min, int32_t max){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  error=copyval(data,obj.name.c_str(),min,max);
 }
 if (min!=max){
 sprintf(buffer,"<input type=\"number\" name=\"%s\" label=\"%s\" value=\"%d\" placeholder=\"%s\" min=\"%d\" max=\"%d\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(),min, max, (obj.required)?" REQUIRED ":"");} else
 {
 sprintf(buffer,"<input type=\"number\" name=\"%s\" label=\"%s\" value=\"%d\" placeholder=\"%s\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(), (obj.required)?" REQUIRED ":"");  
 }
 HTML+=buffer;
 }
bool tSysConfig::edit(htmlproperties obj, float &data, float min, float max){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  error=copyval(data,obj.name.c_str(),min,max);
 }
 if (min!=max){
 sprintf(buffer,"<input type=\"number\" name=\"%s\" label=\"%s\" value=\"%f\" placeholder=\"%s\" min=\"%f\" max=\"%f\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(),min, max, (obj.required)?" REQUIRED ":"");} else
 {
 sprintf(buffer,"<input type=\"number\" name=\"%s\" label=\"%s\" value=\"%f\" placeholder=\"%s\" min=\"%f\" max=\"%f\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(),min, max, (obj.required)?" REQUIRED ":"");  
 }
 HTML+=buffer;
 }

bool tSysConfig::password(htmlproperties obj, char* data, int32_t size, int32_t min, int32_t max){
 // unlike other fields, password auto includes Label & verify dialog 
 char p1[32];
 char p2[32];
 char field[32];
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  // min, max are used to check minimum and maximum length of the password
   error=copyval(p1,obj.name.c_str(),size);
   sprintf(field,"%s_verify",obj.name.c_str());
   error=copyval(p2,field,size);
   if (strcmp(p1,p2)==0){ strncat(data,p1, size); }
}
 sprintf(buffer,"<input type=\"password\" name=\"%s\" label=\"%s\" value=\"%s\" placeholder=\"password\" %s>",obj.name.c_str(),obj.label.c_str(),data,(obj.required)?" REQUIRED ":"");
 HTML+=buffer;
 sprintf(buffer,"<input type=\"password\" name=\"%s_verify\" value=\"%s\" placeholder=\"verify password\" %s>",obj.name.c_str(),data, (obj.required)?" REQUIRED ":"");
 HTML+=buffer;
}
bool tSysConfig::text(htmlproperties obj, char* data, int32_t size, int32_t min, int32_t max){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  error=copyval(data,obj.name.c_str(),size);
 }
 sprintf(buffer,"<textarea name=\"%s\"value=\"%s\" placeholder=\"%s\" size=\"%d\" %s></textarea>",obj.name.c_str(),data,obj.placeholder.c_str(),size, (obj.required)?" REQUIRED ":"");
 HTML+=buffer;
 }
bool tSysConfig::editemail(htmlproperties obj, char* data, int32_t size){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str(),size);
}
 sprintf(buffer,"<input type=\"email\" name=\"%s\" label=\"%s\" value=\"%s\" placeholder=\"%s\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(),(obj.required)?" REQUIRED ":"");
 HTML+=buffer;
}
bool tSysConfig::editurl(htmlproperties obj, char* data, int32_t size){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str(),size);
}
 sprintf(buffer,"<input type=\"url\" name=\"%s\" label=\"%s\" value=\"%s\" placeholder=\"%s\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(), (obj.required)?" REQUIRED ":"");
 HTML+=buffer;
} 
bool tSysConfig::edittel(htmlproperties obj, char* data, int32_t size){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
  error=copyval(data,obj.name.c_str(),size);
 }
 sprintf(buffer,"<input type=\"tel\" name=\"%s\" label=\"%s\" value=\"%s\" placeholder=\"%s\" %s>",obj.name.c_str(),obj.label.c_str(),data,obj.placeholder.c_str(), (obj.required)?" REQUIRED ":"");
 HTML+=buffer;
}
bool tSysConfig::editTime(htmlproperties obj, time_t &data){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
 tmElements_t atime; 
 breakTime(data, atime); 
 //atime.Hour=13;
 //atime.Minute=14;
 sprintf(buffer,"<input type=\"time\" name=\"%s\" label=\"%s\" Value=\"%02d:%02d\" %s>",obj.name.c_str(),obj.label.c_str(),atime.Hour,atime.Minute,(obj.required)?" REQUIRED ":"");
 HTML+=buffer;
}
/*  our select methods require some Javascript magic, the <select> element does not normally carry the value but this is carried here to 
 *  enable our Javascript code to pick it up and set the selected parameter on the correct option value
 *  without doing this, we would have to use a lot more ram to create the list before sending to the web client, this can lead to crashes
 *  when ram is constrained such as on the ESP8266
*/
bool tSysConfig::selectList(htmlproperties obj,int32_t &data){
 if (inSelect) {selectList();}
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
  inSelect=s_int32_t;
 sprintf(buffer,"<select name=\"%s\" label=\"%s\" Value=\"%d\">",obj.name.c_str(),obj.label.c_str(),data);
 HTML+=buffer;
 return true;
 }
bool tSysConfig::selectbyte(htmlproperties obj,byte &data){
 if (inSelect) {selectList();}
 inSelect=s_byte;
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
  sprintf(buffer,"<select name=\"%s\" label=\"%s\" Value=\"%d\">",obj.name.c_str(),obj.label.c_str(),data);
 HTML+=buffer;
 return true;
 }
bool tSysConfig::selectList(htmlproperties obj,float &data){
 if (inSelect) {selectList();}
 inSelect=s_float; 
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
  sprintf(buffer,"<select name=\"%s\" label=\"%s\" Value=\"%f\">",obj.name.c_str(),obj.label.c_str(),data);
 HTML+=buffer;
 return true;
 }
bool tSysConfig::selectList(htmlproperties obj,char* data, int32_t size){
 if (inSelect) {selectList();}
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str(), size);
 }
 inSelect=s_text;
 sprintf(buffer,"<select name=\"%s\" label=\"%s\" Value=\"%s\">",obj.name.c_str(),obj.label.c_str(),data);
 HTML+=buffer; 
 return true;
}
bool tSysConfig::selectList(){
 optiongroup();// ensures option groups are closed
 HTML+=F("</select>"); 
 }
void tSysConfig::label(htmlproperties obj){
 if (obj.label.length()>0){ 
 sprintf(buffer,"<label for=\"%s\" >%s</label><br>",obj.name.c_str(), obj.label.c_str()); 
 HTML+=buffer; 
 }
 }
bool tSysConfig::optiongroup(char const *name){
 optiongroup();// close any previous group
 inOptgroup=true;
 sprintf(buffer,"<optgroup label=\"%s\">",name); 
 HTML+=buffer; 
 }
bool tSysConfig::optiongroup(){
 if (inOptgroup){
 inOptgroup=false;
 HTML+=F("</optgroup>");} 
 }
bool tSysConfig::option(char *data, char *name){
 sprintf(buffer,"<option value=\"%s\" >%s</option>",data,name); 
 HTML+=buffer; 
 }
bool tSysConfig::option(float data, char *name){
 sprintf(buffer,"<option value=\"%f\" >%s</option>",data,name); 
 HTML+=buffer; 
 }
bool tSysConfig::option(int32_t data, char *name){
 sprintf(buffer,"<option value=\"%d\" >%s</option>",data,name); 
 HTML+=buffer; 
 }
 bool tSysConfig::option(byte data, char *name){
 sprintf(buffer,"<option value=\"%d\" >%s</option>",data,name); 
 HTML+=buffer; 
 }
bool tSysConfig::checkbox(htmlproperties obj, bool &data){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
 sprintf(buffer,"<input type=\"checkbox\" name=\"%s\" value=\"%d\" label=\"%s\">",obj.name.c_str(),data,obj.label.c_str());
 HTML+=buffer;  
 }
bool tSysConfig::checkbox(htmlproperties obj, int32_t &data){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
 sprintf(buffer,"<input type=\"checkbox\" name=\"%s\" value=\"%d\" label=\"%s\">",obj.name.c_str(),data,obj.label.c_str());
 HTML+=buffer;  
 }
bool tSysConfig::checkbox(htmlproperties obj, char* data, int32_t size){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str(),size);
 }
 sprintf(buffer,"<input type=\"checkbox\" name=\"%s\" value=\"%d\" label=\"%s\">",obj.name.c_str(),data,obj.label.c_str());
 HTML+=buffer;  
 }
bool tSysConfig::radio(htmlproperties obj, int32_t &data){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str());
 }
 sprintf(buffer,"<input type=\"radio\" name=\"%s\" value=\"%d\" label=\"%s\">",obj.name.c_str(),data,obj.label.c_str());
 HTML+=buffer;  
 }
bool tSysConfig::radio(htmlproperties obj, char *data, int32_t size){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str(), size);
 }
 sprintf(buffer,"<input type=\"radio\" name=\"%s\" value=\"%s\" label=\"%s\">",obj.name.c_str(),data,obj.label.c_str());
 HTML+=buffer;  
 }
 // additional html elements of significant use to us
void tSysConfig::meter(htmlproperties obj, int32_t value, int32_t min, int32_t max){
 // name is not required as this item does not return information
 if (obj.label.length()>0){
 sprintf(buffer,"<meter id=\"%s\" label=\"%s\" value=\"%d\" max=\"%d\"> %d% </meter>",obj.name.c_str(), obj.label.c_str(), value, min, max );} else {
 sprintf(buffer,"<meter id=\"%s\" value=\"%d\" max=\"%d\"> %d% </meter>",obj.name.c_str(), value, min, max );} 
 HTML+=buffer; 
 }
void tSysConfig::meter(htmlproperties obj, float value, float min, float max){
 // name is not required as this item does not return information
 if (obj.label.length()>0){
 sprintf(buffer,"<meter id=\"%s\" label=\"%s\" value=\"%f\" max=\"%f\"> %f% </meter>",obj.name.c_str(), obj.label.c_str(),value, min, max );} else {
 sprintf(buffer,"<meter id=\"%s\" value=\"%f\" max=\"%f\"> %f% </meter>", obj.name.c_str(), value, min, max );}
 HTML+=buffer; 
 }
void tSysConfig::progress(htmlproperties obj, int32_t value, int32_t min, int32_t max){
 // name is not required as this item does not return information
 if (obj.label.length()>0){
 sprintf(buffer,"<progress id=\"%s\" label=\"%s\"value=\"%d\" max=\"%d\"> %d% </progress>",obj.name.c_str(), obj.label.c_str(),value, min, max );} else {
 sprintf(buffer,"<progress id=\"%s\" value=\"%d\" max=\"%d\"> %d% </progress>", obj.name.c_str(), value, min, max );}
 HTML+=buffer; 
 }
void tSysConfig::progress(htmlproperties obj, float value, float min, float max){
 // name is not required as this item does not return information
 if (obj.label.length()>0){
 sprintf(buffer,"<progress id=\"%s\" label=\"%s\" value=\"%f\" max=\"%f\"> %f% </progress>",obj.name.c_str(), obj.label.c_str(), value, min, max );} else {
 sprintf(buffer,"<progress id=\"%s\" value=\"%f\" max=\"%f\"> %f% </progress>",obj.name.c_str(), value, min, max );}
 HTML+=buffer; 
 }
void tSysConfig::details(htmlproperties obj){
 // name is not required as this item does not return information https://www.w3schools.com/tags/tag_button.asp
 sprintf(buffer,"<details><summary>%s</summary><p>%s</p></details>", obj.label.c_str(), obj.value.c_str() );
 HTML+=buffer; 
 }
bool tSysConfig::editrange(htmlproperties obj, int32_t &data, int32_t min, int32_t max){
 if ( server.method() == HTTP_POST ){
  // POST functionality here, includes error handling
   error=copyval(data,obj.name.c_str(), min, max);
 }
  if (min!=max){
  sprintf(buffer,"<input type=\"range\" nam\"%s\" value=\"%d\" min=\"%d\" max=\"%d\">", obj.name.c_str(), data, min, max );} else {
  sprintf(buffer,"<input type=\"range\" nam\"%s\" value=\"%d\">", obj.name.c_str(), data); 
 }
 HTML+=buffer; 
}
// format bytes
String tSysConfig::formatbytes(size_t bytes) {
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

boolean tSysConfig::isinteger(String str){ // check the input is an integer
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
 int32_t pos = str.indexOf(".");
 if (pos==-1){ Serial.println("simple integer"); return isinteger(str); } else {
 s=str.substring(0,pos);
 x=str.substring(pos+1);
 if (isinteger(s)){return isinteger(x);} 
 }
 return false;
} 
boolean tSysConfig::isTime(String str){ // check that the input is a floating point number
 String s;
 String x;
 int32_t pos = str.indexOf(":");
 if (pos==-1){ Serial.println("simple integer"); return false; } else {
 s=str.substring(0,pos);
 x=str.substring(pos+1);
 if (isinteger(s)){return isinteger(x);} 
 }
 return false;
} 
boolean tSysConfig::isIP(String str){// check that the input is an IP4 class address string
 String a; // 4 substrings to define the 4 quadrants of an IP string
 String b;
 String c;
 String d;
 // check for the . character, there needs to be 3 to be a valid ip4 address
 int32_t pos = str.indexOf(".");
 if (pos==-1){ return false;}
 a=str.substring(0,pos);
 int32_t pos2 = str.indexOf(".", pos);
 if (pos2==-1){ return false;}
 b=str.substring(pos+1,pos2);
 pos = str.indexOf(".", pos2);
 if (pos==-1){ return false;}
 c=str.substring(pos2+1,pos);
 d=str.substring(pos+1);
 // check is the substrings are numeric, if not then its not a valid ip
 if (!isinteger(a)){ return false;}
 if (!isinteger(b)){ return false;}
 if (!isinteger(c)){ return false;}
 if (!isinteger(d)){ return false;}
 if (a.toInt()>255){ return false;}
 if (b.toInt()>255){ return false;}
 if (c.toInt()>255){ return false;}
 if (d.toInt()>255){ return false;}
 return true;
}
 // copy values from post function is overloaded and has default parameters, if min-max are equal, range is ignored otherwise range checking is used

int32_t tSysConfig::copyval(byte &var, char const *name, byte min, byte max){
 String V;
 error = 0;
 if (server.hasArg(name)) {
 V=server.arg(name);
 if ((V.length()<1)|(!isinteger(V))){
 errorcount++;
 error=NaN;
 return NaN;
 }
 var=(byte)V.toInt();
 if(max!=min){
 if ((var<=max)&&(var>=min)){
  error = OK;
  return OK;
  }
  errorcount++;
  error = Range;
  return Range;
 }
 return OK; } 
 else{
 errorcount++;
 error = NotFound;
 return NotFound;
 }
}

int32_t tSysConfig::copyval(int32_t &var, char const *name, int32_t min, int32_t max){
 String V;
 error = 0;
 if (server.hasArg(name)) {
 V=server.arg(name);
 if ((V.length()<1)|(!isinteger(V))){
 errorcount++;
 error=NaN;
 return NaN;
 }
 var=V.toInt();
 if(max!=min){
 if ((var<=max)&&(var>=min)){
  error = OK;
  return OK;
  }
  errorcount++;
  error = Range;
  return Range;
 }
 return OK;
 } 
 else{
 errorcount++;
 error = NotFound;
 return NotFound;
 }
}
int32_t tSysConfig::copyval(float &var, char const *name, float min, float max){
 String V;
 error = 0;
 if (server.hasArg(name)) {
 V=server.arg(name);
 if ((V.length()<1)|(!isFloat(V))){
 errorcount++;
 error=NaN;
 return NaN;
 }
 var=V.toFloat();
 if(max!=min){
 if ((var<=max)&&(var>=min)){
  error = OK;
  return OK;
  }
  errorcount++;
  error = Range;
  return Range;
 }
 return OK;
 } 
 else{
 errorcount++;
 error = NotFound;
 return NotFound;
 }
}

int32_t tSysConfig::copyval(time_t &var, char const *name){
 String V;
 String H;
 String M;
 error = 0;
 tmElements_t timeVal; 
 time_t t;
 
if (server.hasArg(name)) {
 V=server.arg(name);
 if (!V.length()) {
  error = InvalidTime;
  return error;
 }
  int pos = V.indexOf(":");
  if (pos==-1){ error=InvalidTime; return error; } else {
  H=V.substring(0,pos);
  M=V.substring(pos+1);

  if (!isinteger(H) || !isinteger(M)){ error=InvalidTime; return error; }
 }
  timeVal.Second=0;
  timeVal.Hour=(uint8_t)H.toInt();
  timeVal.Minute=(uint8_t)M.toInt();
  timeVal.Wday=0;
  timeVal.Day=0;
  timeVal.Month=0;
  timeVal.Year=0;
  // the date variable is offset to 1970 consequently we must compensate to maintain the correct values
  // without the correction, significant time errors occur
  var=86400+makeTime(timeVal);
 } 
 else{
 errorcount++;
 error = NotFound;
 return NotFound;
 }
}

int32_t tSysConfig::copyval(char* var, char const *name, int32_t size, int32_t min){
 ///char buf[128]=""; 
 String V;
 int32_t bytes=0;
 if (server.hasArg(name)) {
  V=server.arg(name);
  if (min>0){if (V.length()<min){
    errorcount++;
    error = RangeLow;
    return RangeLow;}
  }
  if ((V.length()+1)<=size){
  bytes=V.length()+1;} else {bytes=size;}
  bytes=strlcpy(var,V.c_str(), bytes); 
  if (bytes<V.length()){
    errorcount++;
  }
  return bytes; } 
  else{
  errorcount++;
  return NotFound;
 }
}

int32_t tSysConfig::copyIP(IPAddress &var, char const *name){
 String V;
 error = 0;
 if (server.hasArg(name)) {
 V=server.arg(name);
 if ((V.length()<1)|(!isTime(V))){
 errorcount++;
 error=NaN;
 return NaN;
 }
 if ( var.fromString(V) ){
 return OK;
 } 
 }
 else {
 errorcount++;
 error = NotFound;
 return NotFound;
 }
}
void tSysConfig::scanWiFi(){
 bool isConnected=WiFi.status() == WL_CONNECTED;
 int32_t retries=0;
 WiFi.mode(WIFI_AP_STA);
 WiFi.disconnect();
 // scan for available networks
 n = WiFi.scanNetworks(); 
 for (int32_t i = 0; i < n; i++)
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

tSysConfig sysConfig;

