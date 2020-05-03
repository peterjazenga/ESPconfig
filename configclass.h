



ESP8266WebServer server(80);
ESP8266WiFiMulti WiFiMulti;
// variables and constants used for the smartsocket
// -- Initial name of the Device. Used e.g. as SSID of the own Access Point.
const char WiFiname[] = "TheResearchLaboratory";

// -- Initial password to connect to the Device, when it creates an own Access Point.
const char wifiInitialAPpassword[] = "smartsocket";

// -- Initial password to connect to the Device via bluetooth.
const char BLEInitialPassword[] = "1111";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "prototype_2"

// -- When CONFIG_PIN is pulled to ground on startup, the Device will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN D2
//#define STRING_64 64

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).

#define NaN -1
#define NoString -2
#define Range -3
#define NotFound -4
#define OK 0

#define STATUS_PIN LED_BUILTIN
#define RELAY 4

#define STRING_32 32
#define STRING_128 128
#define NUMBER_LEN 16
#define PAGESIZE 512

// heartbeat LED
#define BLINK_PERIOD 250
#define SLOW_BLINK 750
long lastBlinkMillis;
int blinkstate=0;
boolean ledState;

#define SCAN_PERIOD 5000
long lastScanMillis;

#define CONNECT_PERIOD 500
long lastConnectMillis;

// define a structure for on/off timers to control the device activity
typedef struct {
  time_t startTime;
  time_t endTime;
  byte dow;// used as a set of flags to define what day of the week its active
  // the msb is the enable bit for the timer
} ttimer;
typedef struct {
  byte node;
  char nodeName[STRING_32];
} tnodes;

typedef struct {
char WiFiname[STRING_32];
char WiFipassword[STRING_32];
char APname[STRING_32];
char APpassword[STRING_32];
IPAddress ip;
IPAddress gateway;
IPAddress subnet;
byte APchannel;
char NTPserverValue[STRING_128];
char BLEname[STRING_32];
char BLEpassword[STRING_32];
byte configPage;
byte BLEconfig; 
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
char userName[STRING_32];
char userPass[STRING_32];
char userEmail[STRING_128];
char companyName[STRING_32];
char URL[STRING_128];
char NodeName[STRING_32];
//char bufferValue[STRING_32];
time_t regDate;
long regID;
tnodes nodes[16];// nodes to allow master/slave configuration
long checksum;
} dataframe;
dataframe _data;

typedef struct {
byte WiFiname;
byte WiFipassword;
byte APname;
byte APpassword;
byte ip;
byte gateway;
byte subnet;
byte APchannel;
byte NTPserverValue;
byte BLEname;
byte BLEpassword;
byte configPage;
byte BLEconfig; 
byte APconfig; 
byte sensorConfig;
byte timerConfig;
byte TZ; 
byte setPoint; 
byte rangeValue;
byte iceGuard; 
byte min_on;
byte max_on;
byte min_off;
byte OTA;
byte activeHigh;
byte PIDcontrolled; 
byte timers[32];
byte burncount;
byte burntime;
byte userName;
byte userPass;
byte userEmail;
byte companyName;
byte URL;
byte NodeName;
//byte bufferValue;
byte regDate;
byte regID;
byte nodes[16];
byte checksum;
} datacheck;
datacheck _validator;

class tWebObjects {  // The basic web objects classes
  public: // must have methods to support getting the data and posting the data, these are then overridden in child classes
  // by including primitives here, its easier to implement behaviours of forms involving multiple input types
  // functions include a series of validators to test if input is a number, time or date for example  
String errors="";
int errorcount=0;
String HTML;
 
boolean isInteger(String str); 
boolean isFloat(String str);
boolean isTime(String str);
boolean isIP(String str);

int copyVar(char* var, char* name, int size, int min=0);
int copyInt(int &var, char* name);
int copyIntRange(int &var, char* name, int min, int max);
int copyByte(byte &var, char* name);
int copyByteRange(byte &var, char* name, byte min, byte max);
int copyFloat(float &var, char* name);
int copyTime(time_t &var, char* name);
};

class tWebInput: public tWebObjects {
 private:
  tWebObjects vOwner; 
  // various web helper tools are implemented here to validate the input, the generic validator routine is overridden in children to effect the correct validator choice
  // in addition to the validators, there are several generic routines necessary for labeling the editors
  // there are also extended methods to facilitate JSON exchanges instead of HTTP
 void labelfor(char* name, char* id);
 void get();
 void post();
 void getjson();
 void postjson();
};
 
class tEdit: public tWebInput {
  // renders edit inputs, with or without masks
};
class tNumber: public tEdit {
  
};
class tTime: public tEdit {
  
};
class tPassword: public tWebInput {
  
};
class tPasswordConfirmation: public tWebInput {
  
};
class tWebForm: public tWebObjects {
  // webforms must maintain a list of webinput objects so that it can call them to render a web page, a json data frame and to process post data from both web and json POST requests
  // the webform is responsible for rendering the buttons on the form
};

class tWebPage: public tWebObjects {
  
};

class tConfigSys { 
 // this is the master class, it controls the entire config system including loading and saving of configuration data
 // its also responsible for rendering the page title and including persistent page data from local file or web source
 // a system should have at most one configSys file
};



boolean tWebObjects::isInteger(String str){ // check the input is an integer
   for(byte i=0;i<str.length();i++){
      if(!isDigit(str.charAt(i))) {
        return false;
      }
   }
  return true;
} 
boolean tWebObjects::isFloat(String str){ // check that the input is a floating point number
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
boolean tWebObjects::isTime(String str){ // check that the input is a floating point number
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
boolean tWebObjects::isIP(String str){// check that the input is an IP4 class address string
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

int tWebObjects::copyVar(char* var, char* name, int size, int min){
 ///char buf[128]=""; 
 String V;
 int bytes=0;
 if (server.hasArg(name)) {
  V=server.arg(name);
  if (min>0){if (V.length()<min){
  errors += "var ";
  errors += name;
  errors += " too short\r\n";
  errorcount++;
    return Range;}
  }
  if ((V.length()+1)<=size){
  bytes=V.length()+1;} else {bytes=size;}
  bytes=strlcpy(var,V.c_str(), bytes); 
  if (bytes<V.length()){
    errors += "vars ";
    errors += name;
    errors += " wrong size\r\n";
    errorcount++;
  }
  return bytes; } 
  else{
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
}
int tWebObjects::copyInt(int &var, char* name){
 char buf[128]=""; 
 String V;
 if (!server.hasArg(name)) {
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
  V=server.arg(name);
  if (V.length()<1){
  errors += "var ";
  errors += name;
  errors += " not valid\r\n";
  errorcount++;
  return NoString;
  }
 if (!isInteger(V))
 {
  errors += "var ";
  errors += name;
  errors += " NaN\r\n";
  errorcount++;
  return NaN;
  }
  var=V.toInt();
  return OK;
}

int tWebObjects::copyIntRange(int &var, char* name, int min, int max){
 char buf[128]=""; 
 String V;
 if (!server.hasArg(name)) {
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
  V=server.arg(name);
  if (V.length()<1){
  errors += "var ";
  errors += name;
  errors += " not valid\r\n";
  errorcount++;
  return NoString;
  }
 if (!isInteger(V))
 {
  errors += "var ";
  errors += name;
  errors += " NaN\r\n";
  errorcount++;
  return NaN;
  }
  var=V.toInt();
  if ((var<max)&&(var>min)){
    return OK;
  }
  errors += "var ";
  errors += name;
  errors += " out of range\r\n";
  errorcount++;
  return Range;
}
int tWebObjects::copyByte(byte &var, char* name){
 char buf[128]=""; 
 String V;
 if (!server.hasArg(name)) {
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
  V=server.arg(name);
  if (V.length()<1){
  errors += "var ";
  errors += name;
  errors += " not valid\r\n";
  errorcount++;
  return NoString;
  }
 if (!isInteger(V))
 {
  errors += "var ";
  errors += name;
  errors += " NaN\r\n";
  errorcount++;
  return NaN;
  }
  var=V.toInt();
  return OK;
}
int tWebObjects::copyByteRange(byte &var, char* name, byte min, byte max){
 char buf[128]=""; 
 String V;
 if (!server.hasArg(name)) {
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
  V=server.arg(name);
  if (V.length()<1){
  errors += "var ";
  errors += name;
  errors += " not valid\r\n";
  errorcount++;
  return NoString;
  }
 if (!isInteger(V))
 {
  errors += "var ";
  errors += name;
  errors += " NaN\r\n";
  errorcount++;
  return NaN;
  }
  var=V.toInt();
  if ((var<max)&&(var>min)){
    return OK;
  }
  errors += "var ";
  errors += name;
  errors += " out of range\r\n";
  errorcount++;
  return Range;
}
int tWebObjects::copyFloat(float &var, char* name){
 char buf[128]=""; 
 String V;
 if (!server.hasArg(name)) {
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
  V=server.arg(name);
  if (V.length()<1){
  errors += "var ";
  errors += name;
  errors += " not valid\r\n";
  errorcount++;
  return NoString;
  }
 if (!isFloat(V))
 {
  errors += "var ";
  errors += name;
  errors += " NaN\r\n";
  errorcount++;
  return NaN;
  }
  var=V.toFloat();
  return OK;
}
int tWebObjects::copyTime(time_t &var, char* name){
 char buf[128]=""; 
 String V;
 String H;
 String M;
 if (!server.hasArg(name)) {
  errors += "var ";
  errors += name;
  errors += " not found\r\n";
  errorcount++;
  return NotFound;
 }
  V=server.arg(name);
  if (V.length()<1){
  errors += "var ";
  errors += name;
  errors += " not valid\r\n";
  errorcount++;
  return NoString;
  }
 if (!isTime(V))
 {
  errors += "var ";
  errors += name;
  errors += " NaN\r\n";
  errorcount++;
  return NaN;
  }
  int pos = V.indexOf(":");
  if (pos==-1){return NaN; } else {
   H=V.substring(0,pos);
   M=V.substring(pos+1);
   var=(H.toInt() * 3600)+(M.toInt() * 60);
  }
    return OK;
}

void tWebInput::labelfor(char* name, char* id){
 char buf[128]=""; 
 sprintf(buf, "<label for=\"%s\">%s:</label>",id,name);
 HTML += buf; 
}
