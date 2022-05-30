/*
 * V6 Adding Feature to disable slave deep sleep mode for OTA update by providing http status code with 200 for /clientupdate URI
 * 
 */
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <time.h>
#include <WebSerial.h>
#include <ESP8266HTTPClient.h>
#define WIFI_TIMEOUT_MS 20000
#define GARDEN_VERSION "v1"
#include <ArduinoOTA.h>

//unsigned long interval = 30000; // 30 sec

String host = "MasterHost";

unsigned long wifiStatusinterval = 120000; // 2 minutes
unsigned long wifiStatuspreviousMillis = 0;
unsigned long loopinterval = 5000;  // 30 sec
long loopPreviousMillis = 0;

//long soildatapreviousMillis = 0;
long  soildatapreMilDrip = 0;
long  soildatapreMilSprink = 0;

long soildatainterval = 35000;   // if soil data not processed for 35 sec turn off the drip line
//long SoilautoSwitchWaithours = 10800000; // 3 hours to wait and switch from auto to dripline scheduled irrigation
long SoilautoSwitchWaithours = 120000; // 2 minutes for testing

bool wifiAPstatus = false;
AsyncWebServer server(80);

extern "C" {
#include<user_interface.h>
}

RTC_DS3231 rtc;

#define LED 2
const int soil_sensor_pin = A0;

//String current_time;

// Replace with your network credentials
const char* apssid = "sprink7";
const char* appass = "tanulaksha321"; 

const int m_on_d_addr = 15;
const int m_on_s_addr = 16;
const int m_on_h_addr = 17;
const int m_on_m_addr = 18;

const int e_on_d_addr = 19;
const int e_on_s_addr = 20;
const int e_on_h_addr = 21;
const int e_on_m_addr = 22;

const int spr_m_on_d_addr = 23;
const int spr_m_on_s_addr = 24;
const int spr_m_on_h_addr = 25;
const int spr_m_on_m_addr = 26;

const int spr_e_on_d_addr = 27;
const int spr_e_on_s_addr = 28;
const int spr_e_on_h_addr = 29;
const int spr_e_on_m_addr = 30;

const int dripautoIrrStatus_addr = 31;
const int soil_lowerlevel_addr = 32;
const int soil_upperlevel_addr = 33;
const int isoilSensorenabled_addr = 34;
const int sprautoIrrStatus_addr = 35;
const int clientdeepsleepupdate_addr = 36;

const char* i_s_s_k = "irr_schedule_status";
const char* i_t_k = "irr_time";
const char* i_d_k = "irr_duration";

const char* i_f_a_k = "answer";
const char* i_on_d_w_k = "on_demand_release";
const char* i_spr_on_d_w_k = "spr_ondemand_rel";
const char* i_spr_dur_k = "spr_duration";


/*    
Sprinkle Irrigation http parameters
s_i_s_s_k -> sprinkle irrigation schedule status key
s_i_t_k -> sprinkle irrigation time key  
s_i_d_k -> sprinkle irrigation duration key
*/
const char* s_i_s_s_k = "spr_irr_schedule_status";
const char* s_i_t_k = "spr_irr_time";
const char* s_i_d_k = "spr_irr_duration";
const char* w_u_c_s = "update_wifi_conf";

const char* i_s_t_k = "s_time";

String i_s_s_v;
String i_t_v;
String i_d_v;
String i_f_a_v;
String i_s_t_v;

int mOnHour;
int mOnMin;
int eOnHour;
int eOnMin;
int mOffHour;
int mOffMin;
int eOffHour;
int eOffMin;
int mor_dur;
int eve_dur;

bool m_on = false;
bool e_on = false;

const int driprelay = 2;
const int spr_motor_relay = 14;
bool driprelaystatus = false;
bool motorrelaystatus = false;
int timer_duration;

int *drip_sch_arry;
String drip = "Drip";
String sprinkle = "Sprinkle";

bool debug_msg = false;

int dripautoirrstatus;
int sprautoirrstatus;
bool isoilSensorenabled = false;

int m_irr_status;
int e_irr_status;
int spr_m_irr_status;
int spr_e_irr_status;
int clndeepsleepstatus;

template <class T> int EEPROM_writeAnything(int ee, const T& value);
template <class T> int EEPROM_readAnything(int ee, T& value);

struct wifiConfig 
{
  boolean isAP = false;
  char configured[10] = GARDEN_VERSION;
  char wifissid[30];
  char wifipass[30];
  char apssid[30];
  char appass[30];
  char soilHost[40];
} configuration;

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  EEPROM.begin(512);
  readConfigFromEEPROM();
  String apssid = String(configuration.apssid);
  Serial.println("-------------------------------------");
  Serial.println(apssid.isEmpty());
  Serial.println("-------------------------------------");
  if (configuration.isAP) {
    if (!(String(configuration.configured) == String(GARDEN_VERSION))) {
      Serial.println("Eeprom is null updating AP details");
      String("sprinkle7").toCharArray(configuration.apssid, 30);
      String("tanulaksha321").toCharArray(configuration.appass, 30);
      saveConfigToEEPROM();
       
    }
  }

  pinMode(driprelay, OUTPUT);
  pinMode(spr_motor_relay, OUTPUT);
  digitalWrite(driprelay, HIGH);
  digitalWrite(spr_motor_relay, HIGH);
  driprelaystatus=false;
  motorrelaystatus = false;
  
  m_irr_status=read_eeprom(m_on_s_addr);
  e_irr_status=read_eeprom(e_on_s_addr);
  spr_m_irr_status=read_eeprom(spr_m_on_s_addr);
  spr_e_irr_status=read_eeprom(spr_e_on_s_addr);
  dripautoirrstatus = read_eeprom(dripautoIrrStatus_addr);
  sprautoirrstatus = read_eeprom(sprautoIrrStatus_addr);
  clndeepsleepstatus = read_eeprom(clientdeepsleepupdate_addr);
  
  rtc.begin();
  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  WiFi.hostname("master.tanu.com");
  initWiFi();
  delay(100);

// OTA start here
ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

//OTA END here

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("OFF hit.");
      int params = request->params();
      Serial.printf("%d params sent in\n", params);
    if ( request->hasParam(i_s_s_k) && request->hasParam(i_f_a_k)) {
      i_s_s_v= request->getParam(i_s_s_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_s_s_v == "mor"  &&  i_f_a_v == "on") {
        update_eeprom(m_on_s_addr,1);
        m_irr_status = 1;
      }
      else if ( i_s_s_v == "mor"  &&  i_f_a_v == "off") {
        update_eeprom(m_on_s_addr,0);
        m_irr_status = 0;
        }
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "on") {
        update_eeprom(e_on_s_addr,1);
        e_irr_status = 1;
        }  
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "off") {
        update_eeprom(e_on_s_addr,0);
        e_irr_status = 0;
        }   
        request->redirect("/");           
    }
    if ( request->hasParam(s_i_s_s_k) && request->hasParam(i_f_a_k)) {
      i_s_s_v= request->getParam(s_i_s_s_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_s_s_v == "mor"  &&  i_f_a_v == "on") {
        update_eeprom(spr_m_on_s_addr,1);
        spr_m_irr_status = 1;
      }
      else if ( i_s_s_v == "mor"  &&  i_f_a_v == "off") {
        update_eeprom(spr_m_on_s_addr,0);
        spr_m_irr_status = 0;
        }
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "on") {
        update_eeprom(spr_e_on_s_addr,1);
        spr_e_irr_status = 1;
        }  
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "off") {
        update_eeprom(spr_e_on_s_addr,0);
        spr_e_irr_status = 0;
        }   
        request->redirect("/");           
    }    
    if (request->hasParam(s_i_t_k) && request->hasParam(i_f_a_k)) {
      i_t_v= request->getParam(s_i_t_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      int _hour = i_f_a_v.substring(0,2).toInt();
      int _min = i_f_a_v.substring(3,5).toInt();      
      if ( i_t_v == "mor" ) {
        update_eeprom(spr_m_on_h_addr,_hour);
        update_eeprom(spr_m_on_m_addr,_min);
      }
      else if ( i_t_v == "eve" ){
        update_eeprom(spr_e_on_h_addr,_hour);
        update_eeprom(spr_e_on_m_addr,_min);
        }
        request->redirect("/");
    }     
    if (request->hasParam(s_i_d_k) && request->hasParam(i_f_a_k)) {
      i_d_v= request->getParam(s_i_d_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();    
      if ( i_d_v == "mor" ) {
        update_eeprom(spr_m_on_d_addr,i_f_a_v.toInt());
      }
      else if ( i_d_v == "eve" ){
        update_eeprom(spr_e_on_d_addr,i_f_a_v.toInt());
        }
        request->redirect("/");
    }     
    if (request->hasParam(i_t_k) && request->hasParam(i_f_a_k)) {
      i_t_v= request->getParam(i_t_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      int _hour = i_f_a_v.substring(0,2).toInt();
      int _min = i_f_a_v.substring(3,5).toInt();      
      if ( i_t_v == "mor" ) {
        update_eeprom(m_on_h_addr,_hour);
        update_eeprom(m_on_m_addr,_min);
      }
      else if ( i_t_v == "eve" ){
        update_eeprom(e_on_h_addr,_hour);
        update_eeprom(e_on_m_addr,_min);
        }
        request->redirect("/");
    }    
    if (request->hasParam(i_d_k) && request->hasParam(i_f_a_k)) {
      i_d_v= request->getParam(i_d_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();    
      if ( i_d_v == "mor" ) {
        update_eeprom(m_on_d_addr,i_f_a_v.toInt());
      }
      else if ( i_d_v == "eve" ){
        update_eeprom(e_on_d_addr,i_f_a_v.toInt());
        }
        request->redirect("/");
    }
    if ( request->hasParam(i_s_t_k) && request->hasParam(i_f_a_k)) {
      i_s_t_v = request->getParam(i_s_t_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      logging_msg("Update webserver time :",false);
      logging_msg(i_f_a_v,true);
      Serial.println("Update webserver time :");
      Serial.print(i_f_a_v);
      Serial.println();
      update_server_time(i_f_a_v);
      request->redirect("/");
                  
    }
    if ( request->hasParam(i_on_d_w_k) && request->hasParam(i_f_a_k)) {
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_f_a_v == "on" ){
        start_iopin(driprelay);
        logging_msg("Drip Line Ondemand water started: ",true);
        //digitalWrite(driprelay, LOW);
        driprelaystatus = true;
      }
      else if ( i_f_a_v == "off" ){
        stop_iopin(driprelay);
        logging_msg("Drip Line Ondemand water Stopped: ",true);
        //digitalWrite(driprelay, HIGH);
        driprelaystatus = false;
      }
      request->redirect("/");
                  
    }    
    if ( request->hasParam(i_spr_on_d_w_k) && request->hasParam(i_f_a_k)) {
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_f_a_v == "on" ){
        start_iopin(spr_motor_relay);
        //digitalWrite(spr_motor_relay,LOW);
        motorrelaystatus = true;
        logging_msg("Sprinkle Ondemand water started: ",true);
        
      }
      else if ( i_f_a_v == "off" ){
        stop_iopin(spr_motor_relay);
        //digitalWrite(spr_motor_relay,HIGH);
        motorrelaystatus = false;
        logging_msg("Sprinkle Ondemand water Stopped: ",true);
      }
      request->redirect("/");
                  
    }     
    if (request->hasParam(i_spr_dur_k) && request->hasParam(i_f_a_k)) {
      i_d_v= request->getParam(i_spr_dur_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();    
      if ( i_d_v == "mor" ) {
        update_eeprom(spr_m_on_d_addr,i_f_a_v.toInt());
      }
      else if ( i_d_v == "eve" ){
        update_eeprom(spr_e_on_d_addr,i_f_a_v.toInt());
        }
        request->redirect("/");
    }
    if ( request->hasParam("soil_data_host") && request->hasParam(i_f_a_k)) {
        i_d_v= request->getParam(i_f_a_k)->value();
        Serial.println("Received data host is :"+i_d_v);
        i_d_v.toCharArray(configuration.soilHost,40);
        saveConfigToEEPROM();
        request->redirect("/");
    }

    if ( request->hasParam("soil_sensor_status") && request->hasParam(i_f_a_k)) {
        i_d_v= request->getParam(i_f_a_k)->value();
        Serial.println("Soil Sensor status :"+i_d_v);
        if ( i_d_v == "on" ) {
          update_eeprom(isoilSensorenabled_addr,1);
          isoilSensorenabled = true;
        }
        else if ( i_d_v == "off" ) {
          update_eeprom(isoilSensorenabled_addr,0);
          isoilSensorenabled = false;
        }
        request->redirect("/");
    }    

    if ( request->hasParam("soil_drip_auto_irr_status") && request->hasParam(i_f_a_k)) {
        i_d_v= request->getParam(i_f_a_k)->value();
        Serial.println("Soil Sensor Drip auto irrigation status :"+i_d_v);
        if ( i_d_v == "on" ) {
          update_eeprom(dripautoIrrStatus_addr,1);
          dripautoirrstatus = 1;
        }
        else if ( i_d_v == "off" ) {
          update_eeprom(dripautoIrrStatus_addr,0);
          dripautoirrstatus = 0;
          stop_iopin(driprelay);
        }
        request->redirect("/");
    } 
    if ( request->hasParam("soil_spr_auto_irr_status") && request->hasParam(i_f_a_k)) {
        i_d_v= request->getParam(i_f_a_k)->value();
        Serial.println("Soil Sensor Sprinkle auto irrigation status :"+i_d_v);
        if ( i_d_v == "on" ) {
          update_eeprom(sprautoIrrStatus_addr,1);
          sprautoirrstatus = 1;
        }
        else if ( i_d_v == "off" ) {
          update_eeprom(sprautoIrrStatus_addr,0);
          sprautoirrstatus = 0;
          stop_iopin(spr_motor_relay);
        }
        request->redirect("/");
    } 
   
    if ( request->hasParam("soil_moisture_l_l") && request->hasParam(i_f_a_k)) {
        i_f_a_v = request->getParam(i_f_a_k)->value(); 
        Serial.println("Updating Soil sensor lower level value :"+i_f_a_v);
        update_eeprom(soil_lowerlevel_addr,i_f_a_v.toInt());
        request->redirect("/");
    } 

    if ( request->hasParam("soil_moisture_h_l") && request->hasParam(i_f_a_k)) {
        i_f_a_v= request->getParam(i_f_a_k)->value();
        Serial.println("Updating Soil sensor Upper level value :"+i_f_a_v);
        update_eeprom(soil_upperlevel_addr,i_f_a_v.toInt());
        request->redirect("/");
    }     
    //request->send(SPIFFS, "/index.html", "text/html",processor);
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
   server.on("/restartESP", HTTP_POST, [](AsyncWebServerRequest *request){

    if (request->hasParam("update_wifi_conf",true) && request->hasParam("wifissid",true)) {
        String wifiname = request->getParam("wifissid",true)->value();
        String pass_name = request->getParam("wifipass",true)->value();  
        //String wifimode = request->getParam("wifimode",true)->value();  
        Serial.println("Post Method Received ssid : "+wifiname);
        
        if(configuration.isAP) {
        wifiname.toCharArray(configuration.apssid,30);
        pass_name.toCharArray(configuration.appass,30);
        }
        else {
          wifiname.toCharArray(configuration.wifissid,30);
          pass_name.toCharArray(configuration.wifipass,30);
        }
        saveConfigToEEPROM();
        
    }    
    if (request->hasParam("update_wifi_mode",true) && request->hasParam("wifimode",true)) {
        String wifimode = request->getParam("wifimode",true)->value();
        Serial.println("Post Method Received Wifi Mode : "+wifimode);        
        if (wifimode == "on" ){
        configuration.isAP = true;
        }
        else {
          configuration.isAP = false;
        }
        saveConfigToEEPROM();
        restartesp();
    }
   });
   server.on("/soilsensor", HTTP_POST, [](AsyncWebServerRequest *request){
    String soil_moisture_p_l;
    String soilchannel;
    String soilhost;
    if ( request->hasParam("soil_moisture_p_l",true) &&  request->hasParam("relaychannel",true) && request->hasParam("host",true) ) {
        soil_moisture_p_l = request->getParam("soil_moisture_p_l",true)->value(); 
        soilchannel = request->getParam("relaychannel",true)->value();
        soilhost = request->getParam("host",true)->value();
        soil_sensor_auto_irrigation(soil_moisture_p_l.toInt(),soilhost,soilchannel.toInt());
        request->send(200);
    }
    else{
      logging_msg("Invalid /soilsensor post data recevied ",true);
      Serial.println("Invalid /soilsensor post data recevied ");
      //request->send(500);
    }
    //soil_sensor_auto_irrigation(soil_moisture_l_l.toInt(),soil_moisture_h_l.toInt(),soil_moisture_p_l.toInt());
   });
   server.on("/clientupdate", HTTP_POST, [](AsyncWebServerRequest *request){
      int clndeepsleepstatus = read_eeprom(clientdeepsleepupdate_addr);
      if ( clndeepsleepstatus == 1 ) {
          request->send(200);
      }
      else {
          request->send(202);
      }
    
   });
   server.on("/clientdeepsleep", HTTP_POST, [](AsyncWebServerRequest *request){
    if ( request->hasParam("client_deep_sleep_status",true) && request->hasParam(i_f_a_k,true)) {
        i_d_v= request->getParam(i_f_a_k,true)->value();
        Serial.println("client deep_sleep status :"+i_d_v);
        if ( i_d_v == "on" ) {
          update_eeprom(clientdeepsleepupdate_addr,1);
          clndeepsleepstatus = 1;
        }
        else if ( i_d_v == "off" ) {
          update_eeprom(clientdeepsleepupdate_addr,0);
          clndeepsleepstatus = 0;
        }
        //request->send(200);
        request->redirect("/");
    }      
    
   });   
   WebSerial.begin(&server);
   WebSerial.msgCallback(recvMsg);
   server.begin();
}
void loop(){
 //DateTime now = rtc.now();
//int sec1 = now.second() % 5;
//  Serial.println(sec1);
//  Serial.println(now.second(), DEC);
//if ( sec1 == 0 ) {
  drip_irrigation();
  sprinkle_irrigation();
  client_status();
  WifiStatus();
  check_soil_sensor_data();
  monitor_soil_auto_irrigation_data();
  ArduinoOTA.handle();
  delay(5000);
//}
}
void update_server_time(String dateTime1) {
  int y = dateTime1.substring(0,4).toInt();
  int mn = dateTime1.substring(5,7).toInt();
  int d = dateTime1.substring(8,10).toInt();
  int h = dateTime1.substring(11,13).toInt();
  int m = dateTime1.substring(14,16).toInt();
  WebSerial.print(y);
  WebSerial.print("-");
  WebSerial.print(mn);
  WebSerial.print("-");
  WebSerial.print(d);
  WebSerial.print(":");
  WebSerial.print(h);
  WebSerial.print(":");
  WebSerial.println(m);
  rtc.adjust(DateTime(y, mn, d, h, m, 0));
  //return true;
  //ESP.restart();
  
}
String processor(const String& var){
 //int m_checked=read_eeprom(m_on_s_addr);
// int e_checked=read_eeprom(e_on_s_addr);
 int m_s_h = read_eeprom(m_on_h_addr);
 int m_s_m = read_eeprom(m_on_m_addr);
 int m_duration = read_eeprom(m_on_d_addr);
 int e_s_h = read_eeprom(e_on_h_addr);
 int e_s_m = read_eeprom(e_on_m_addr);
 int e_duration = read_eeprom(e_on_d_addr);
    
 //int spr_m_checked=read_eeprom(spr_m_on_s_addr);
 //int spr_e_checked=read_eeprom(spr_e_on_s_addr);
 
 int spr_m_s_h = read_eeprom(spr_m_on_h_addr);
 int spr_m_s_m = read_eeprom(spr_m_on_m_addr);
 int spr_m_duration = read_eeprom(spr_m_on_d_addr);
 int spr_e_s_h = read_eeprom(spr_e_on_h_addr);
 int spr_e_s_m = read_eeprom(spr_e_on_m_addr);
 int spr_e_duration = read_eeprom(spr_e_on_d_addr);

 int soil_lowerlevel = read_eeprom(soil_lowerlevel_addr);
 int soil_upperlevel = read_eeprom(soil_upperlevel_addr);
 //int dripautoirrstatus = read_eeprom(dripautoIrrStatus_addr);
 int isoilSensorenabled = read_eeprom(isoilSensorenabled_addr);
  clndeepsleepstatus = read_eeprom(clientdeepsleepupdate_addr);
 
if(var == "M_ON_CHECKED"){
    if (m_irr_status == 1 ){
      return "checked";
    }
    else if ( m_irr_status == 0 ){
      return "";
    }
  }
  else if (var == "M_OFF_CHECKED"){
    if ( m_irr_status == 0 ){
      return "checked";
    }
    else if ( m_irr_status == 1 ){
      return "";
    }
  }
  
 else if (var == "MORNING_SCHEDULE"){

    String m_s_t;
    if ( m_s_h <= 9 && m_s_m <= 9 ) {
      m_s_t = "0"+String(m_s_h)+":0"+String(m_s_m);
    }
    else if ( m_s_h <= 9 && m_s_m > 9 ){
      m_s_t="0"+String(m_s_h)+":"+String(m_s_m);
    }
    else if ( m_s_h > 9 && m_s_m <= 9 ){
      m_s_t=String(m_s_h)+":0"+String(m_s_m);
    }
    else {
      m_s_t=String(m_s_h)+":"+String(m_s_m);
    }
    logging_msg("Morning schedule:"+m_s_t,true);
    return m_s_t;
  }
  else if(var == "E_ON_CHECKED"){
    if ( e_irr_status == 1 ){
      return "checked";
    }
    else if ( e_irr_status == 0 ){
      return "";
    }
  }
  else if ( var == "E_OFF_CHECKED"){
    if ( e_irr_status == 0 ){
      return "checked";
    }
    else if ( e_irr_status == 1 ){
      return "";
    }
  }
  else if (var == "EVENING_SCHEDULE"){
    String e_s_t;
    if ( e_s_h <= 9 && e_s_m <= 9 ) {
      e_s_t = "0"+String(e_s_h)+":0"+String(e_s_m);
    }
    else if ( e_s_h <= 9 && e_s_m > 9 ){
      e_s_t="0"+String(e_s_h)+":"+String(e_s_m);
    }
    else if ( e_s_h > 9 && e_s_m <= 9 ){
      e_s_t=String(e_s_h)+":0"+String(e_s_m);
    }
    else {
      e_s_t=String(e_s_h)+":"+String(e_s_m);
    }
    logging_msg("Evening schedule:"+e_s_t,true);
    return e_s_t;    
  }      
  else if (var == "MOR_DURATION"){
    return String(m_duration);
  }  
  else if (var == "EVE_DURATION"){
    return String(e_duration);
  }  
  else if (var == "SERVER_TIME"){
    char buf[] = "DD-MM-YY:hh:mm:ss";
    DateTime now = rtc.now();
    String current_time=String(now.toString(buf));
    return String(current_time);
  }
  else if (var == "R_ON_CHECKED"){
    String r_relay = "";
    if (driprelaystatus) {
      r_relay = "checked";
    }
    return r_relay;
  }
  else if (var == "R_OFF_CHECKED"){
    String r_relay = "checked";
    if (driprelaystatus) {
      r_relay = "";
    }
    return r_relay;
  }

  else if (var == "SPR_R_ON_CHECKED"){
    String spr_relay = "";
    if (motorrelaystatus) {
      spr_relay = "checked";
    }
    return spr_relay;
  }
  else if (var == "SPR_R_OFF_CHECKED"){
    String spr_relay = "checked";
    if (motorrelaystatus) {
      spr_relay = "";
    }
    return spr_relay;
  }    
// Start here
if(var == "SPR_M_ON_CHECKED"){
    if (spr_m_irr_status == 1 ){
      return "checked";
    }
    else if ( spr_m_irr_status == 0 ){
      return "";
    }
  }
  else if (var == "SPR_M_OFF_CHECKED"){
    if ( spr_m_irr_status == 0 ){
      return "checked";
    }
    else if ( spr_m_irr_status == 1 ){
      return "";
    }
  }
  
 else if (var == "SPR_MORNING_SCHEDULE"){
    String m_s_t;
    if ( spr_m_s_h <= 9 && spr_m_s_m <= 9 ) {
      m_s_t = "0"+String(spr_m_s_h)+":0"+String(spr_m_s_m);
    }
    else if ( spr_m_s_h <= 9 && spr_m_s_m > 9 ){
      m_s_t="0"+String(spr_m_s_h)+":"+String(spr_m_s_m);
    }
    else if ( spr_m_s_h > 9 && spr_m_s_m <= 9 ){
      m_s_t=String(spr_m_s_h)+":0"+String(spr_m_s_m);
    }
    else {
      m_s_t=String(spr_m_s_h)+":"+String(spr_m_s_m);
    }
    logging_msg("SPR Morning schedule:"+m_s_t,true);
    return m_s_t;
  }
  else if(var == "SPR_E_ON_CHECKED"){
    if ( spr_e_irr_status == 1 ){
      return "checked";
    }
    else if ( spr_e_irr_status == 0 ){
      return "";
    }
  }
  else if ( var == "SPR_E_OFF_CHECKED"){
    if ( spr_e_irr_status == 0 ){
      return "checked";
    }
    else if ( spr_e_irr_status == 1 ){
      return "";
    }
  }

//
if(var == "CLT_DEEP_ON_CHECKED"){
    if ( clndeepsleepstatus == 1 ){
      return "checked";
    }
    else if ( clndeepsleepstatus == 0 ){
      return "";
    }
  }
  else if ( var == "CLT_DEEP_OFF_CHECKED"){
    if ( clndeepsleepstatus == 0 ){
      return "checked";
    }
    else if ( clndeepsleepstatus == 1 ){
      return "";
    }
  }
//
  else if (var == "SPR_EVENING_SCHEDULE"){
    String e_s_t;
    if ( spr_e_s_h <= 9 && spr_e_s_m <= 9 ) {
      e_s_t = "0"+String(spr_e_s_h)+":0"+String(spr_e_s_m);
    }
    else if ( spr_e_s_h <= 9 && spr_e_s_m > 9 ){
      e_s_t="0"+String(spr_e_s_h)+":"+String(spr_e_s_m);
    }
    else if ( spr_e_s_h > 9 && spr_e_s_m <= 9 ){
      e_s_t=String(spr_e_s_h)+":0"+String(spr_e_s_m);
    }
    else {
      e_s_t=String(spr_e_s_h)+":"+String(spr_e_s_m);
    }
    logging_msg("Evening schedule:"+e_s_t,true);
    return e_s_t;    
  }
  else if (var == "SPR_E_DURATION"){
    return String(spr_e_duration);
  }      
  else if (var == "SPR_M_DURATION"){
    return String(spr_m_duration);
  }  
  if (var == "WIFI_AP_ON") {
    if (configuration.isAP) {
      return "checked";
    }
    else {
      return "";
    }
  }
  if (var == "WIFI_AP_OFF") {
    if (configuration.isAP) {
      return "";
    }
    else {
      return "checked";
    }
  }  
  if (var == "WIFI_SSID") {
      Serial.println("wifissid is :"+String(configuration.wifissid));
      Serial.println("wifissid is :"+String(configuration.wifipass));
    if (configuration.isAP) {
      return configuration.apssid;
    }
    else {
      return configuration.wifissid;
    }
  }  
  if (var == "WIFI_PASS") {
    if (configuration.isAP) {
       return configuration.appass;
    }
    else {
        return configuration.wifipass;
    }
  }  
// End here
  if (var == "SOIL_DATA_HOST_URL") {
    return configuration.soilHost;
  } 
  if (var == "SOIL_MOISTURE_L_L") {
    return String(soil_lowerlevel);
    
  }   
  if (var == "SOIL_MOISTURE_H_L") {
    return String(soil_upperlevel);
    
  }     
  if (var == "SOIL_MOISTURE_LEVEL") {
     int soildata = soil_sensor_read_new();
      return String(soildata);
    
  }  
  if (var == "DRIP_SOIL_AUTO_ON_CHECKED") {
    if (dripautoirrstatus) {
      return "checked";
    }
    else {
      return "";
    }
  }  
  if (var == "DRIP_SOIL_AUTO_OFF_CHECKED") {
    if (dripautoirrstatus) {
      return "";
    }
    else {
      return "checked";
    }
  }

     if (var == "SPR_SOIL_AUTO_ON_CHECKED") {
    if (sprautoirrstatus) {
      return "checked";
    }
    else {
      return "";
    }
  }  
  if (var == "SPR_SOIL_AUTO_OFF_CHECKED") {
    if (sprautoirrstatus) {
      return "";
    }
    else {
      return "checked";
    }
 }
  if (var == "SOIL_ON_CHECKED") {
    if (isoilSensorenabled) {
      return "checked";
    }
    else {
      return "";
    }
  }  
  if (var == "SOIL_OFF_CHECKED") {
    if (isoilSensorenabled) {
      return "";
    }
    else {
      return "checked";
    }
  }  
}

void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
    WebSerial.println(d);
  if (d == "ON"){
    digitalWrite(LED, LOW);
  }
  if (d=="OFF"){
    digitalWrite(LED, HIGH);
  }
  if (d=="debugon"){
    debug_msg = true;
  }
  if (d=="debugoff"){
    debug_msg = false;
  }

}
int read_eeprom(int add){
  int readInt = EEPROM.read(add);
  return readInt;
}
void update_eeprom(int address,int value){
  EEPROM.write(address, value);
  EEPROM.commit();
  logging_msg("Updated address and value :"+String(address)+":"+String(value),true);
}
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

void drip_irrigation() {
  Serial.println("Scheduled drip irrigation called");
  irrigation_on(driprelay, driprelaystatus, m_on_h_addr, m_on_m_addr, e_on_h_addr, e_on_m_addr, m_on_d_addr, e_on_d_addr, m_irr_status, e_irr_status,drip);
}
void sprinkle_irrigation() {
  Serial.println("Scheduled Sprinkle irrigation called");
  irrigation_on(spr_motor_relay, motorrelaystatus, spr_m_on_h_addr, spr_m_on_m_addr, spr_e_on_h_addr, spr_e_on_m_addr, spr_m_on_d_addr, spr_e_on_d_addr, spr_m_irr_status, spr_e_irr_status,sprinkle);
}

void irrigation_on(int relayNum,bool &relaystatus,int m_h_add, int m_m_add,int e_h_add,int e_m_add,int m_d_add,int e_d_add,int &m_irr_status, int &e_irr_status,String msg){
  drip_sch_arry = getSchedule(m_h_add, m_m_add, e_h_add, e_m_add, m_d_add, e_d_add );
  Serial.println("irrigation_on method called");
  mOnHour = *(drip_sch_arry+0);
  mOnMin = *(drip_sch_arry+1);
  mOffHour = *(drip_sch_arry+2);
  mOffMin = *(drip_sch_arry+3);

  eOnHour = *(drip_sch_arry+4);
  eOnMin = *(drip_sch_arry+5);
  eOffHour = *(drip_sch_arry+6);
  eOffMin = *(drip_sch_arry+7);
  DateTime now = rtc.now();
  if ( m_irr_status == 1 ) {
    if(now.hour() == mOnHour && now.minute() == mOnMin){
      //digitalWrite(relayNum, LOW);
      start_iopin(relayNum);
      logging_msg(msg+": Morning Irrigation Started: ",true);
      logging_msg(msg+": Morning Irrigation Endtime: "+String(mOffHour)+":"+String(mOffMin),true);
      //relaystatus = true;
    }
    else if(now.hour() == mOffHour && now.minute() == mOffMin) {
      stop_iopin(relayNum);
      //digitalWrite(relayNum, HIGH);
      logging_msg(msg+": Morining Irrigation stopped",true);
     //relaystatus = false;
    }
  }
  else {
    logging_msg(msg+": Morning irrigation off",true);
  }
  if ( e_irr_status == 1 ) {
    if(now.hour() == eOnHour && now.minute() == eOnMin){
     //digitalWrite(relayNum, LOW);
      start_iopin(relayNum);
      logging_msg(msg+": Evening Irrigation Started: ",true);
      logging_msg(msg+": Evening Irrigation Endtime: "+String(eOffHour)+":"+String(eOffMin),true);
      //relaystatus = true;
    }
    else if(now.hour() == eOffHour && now.minute() == eOffMin) {
      //digitalWrite(relayNum, HIGH);
      stop_iopin(relayNum);
      logging_msg(msg+": Evening Irrigation stopped",true);
      //relaystatus = false;
    }
  }
  else {
    logging_msg(msg+" Evening irrigation off",true);
  }  
}

int * getSchedule(int mor_h_add,int mor_m_add,int eve_h_add,int eve_m_add,int mor_dur_add,int eve_dur_add){
  static int sch_arry[8];
  
   mOnHour = read_eeprom(mor_h_add);
   mOnMin = read_eeprom(mor_m_add);
   eOnHour = read_eeprom(eve_h_add);
   eOnMin = read_eeprom(eve_m_add);
   mor_dur = read_eeprom(mor_dur_add);
   eve_dur = read_eeprom(eve_dur_add);
   
   int m_total_minutes = ((mOnHour * 60) + mOnMin + mor_dur);
   mOffHour = m_total_minutes / 60;
   mOffMin = m_total_minutes % 60;
   
   int e_total_minutes = ((eOnHour * 60) + eOnMin + eve_dur);
   eOffHour = e_total_minutes / 60;
   eOffMin = e_total_minutes % 60;
   
   sch_arry[0] = mOnHour;
   sch_arry[1] = mOnMin;
   sch_arry[2] = mOffHour;
   sch_arry[3] = mOffMin;
   sch_arry[4] = eOnHour;
   sch_arry[5] = eOnMin;
   sch_arry[6] = eOffHour;
   sch_arry[7] = eOffMin;

   return sch_arry;
}

void start_iopin(int pin) {
  digitalWrite(pin, LOW);
  if ( pin == 2 ) {
    driprelaystatus = true;
  }
  else if ( pin == 14 ) {
    motorrelaystatus = true;
  }
  
}

void stop_iopin(int pin) {
  
  digitalWrite(pin, HIGH);
  if ( pin == 2 ) {
    driprelaystatus = false;
  }
  else if ( pin == 14 ) {
    motorrelaystatus = false;
  }
  
}

void banner_msg( String msg) {
  if (debug_msg) {
    DateTime now = rtc.now();
    char buf2[] = "DD/MM/YY-hh:mm:ss";
    WebSerial.println(String(now.toString(buf2))+":"+msg);
  }
}
void logging_msg( String msg,bool newline) {
 // WebSerial.println("Recevied msg :"+msg);
  if (debug_msg) {
    DateTime now = rtc.now();
    char buf2[] = "DD/MM/YY-hh:mm:ss";
    if (newline) {
      WebSerial.println(String(now.toString(buf2))+":"+msg);
    }
    else {
      WebSerial.print(String(now.toString(buf2))+":"+msg);
    }
  }
}
void saveConfigToEEPROM()
{
  EEPROM_writeAnything(100, configuration);
  //EEPROM_writeAnything(200, soildatahostconfig);
}
void readConfigFromEEPROM()
{
  EEPROM_readAnything(100, configuration);
  //EEPROM_readAnything(200, soildatahostconfig);
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  EEPROM.commit();
  return i;
}

void client_status() 
{
    auto client_count = wifi_softap_get_station_num();
    
    Serial.printf("Total devices connected = %d\n", client_count);
    

    auto i = 1;
    struct station_info *station_list = wifi_softap_get_station_info();
    while (station_list != NULL) {
        
        auto station_ip = IPAddress((&station_list->ip)->addr).toString().c_str();
        char station_mac[18] = {0};
        sprintf(station_mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(station_list->bssid));
        Serial.printf("%d. %s %s", i++, station_ip, station_mac);
        station_list = STAILQ_NEXT(station_list, next);
        Serial.println();
    }
    wifi_softap_free_station_info();
}
void restartesp(){
  //ESP.reset();
  ESP.restart();
}

void soil_sensor_auto_irrigation(int soilrawValue,String host,int relaynum) {
  int dripautoirrstatus = read_eeprom(dripautoIrrStatus_addr);
  DateTime now = rtc.now();
  //bool relaystatus;
  int autoirrstatus;
  String irr_type;
  if ( relaynum == 2 ) {
   soildatapreMilDrip = millis();
   autoirrstatus = read_eeprom(dripautoIrrStatus_addr);
   irr_type =  "Drip";
    //relaystatus  = &driprelaystatus;
  }
  if ( relaynum == 14 ) {
    soildatapreMilSprink = millis();
    autoirrstatus = read_eeprom(sprautoIrrStatus_addr);
    irr_type =  "Sprinkle";
    //relaystatus  = &motorrelaystatus;
  }

  if( autoirrstatus == 1 ) {
    int soil_moisture_l_l = read_eeprom(soil_lowerlevel_addr);
    int soil_moisture_h_l = read_eeprom(soil_upperlevel_addr);
    int  soil_moisture_p_l = ( 100.00 - ( (soilrawValue/1023.00) * 100.00 ) );
    logging_msg("Soildata values|Host|relay|LL|UL|PL :"+host+"|"+String(relaynum)+"|"+String(soil_moisture_l_l)+"|"+String(soil_moisture_h_l)+"|"+String(soil_moisture_p_l),true);
    if ( soil_moisture_p_l <= 0 ){
      Serial.println("Error :Soil Senor value not reliable.");
      logging_msg("Error :Soil Senor value not reliable.",true);
    }
    else if ( soil_moisture_p_l  <= soil_moisture_l_l )
    {      
      DateTime now = rtc.now();
      if ( now.hour() >= 10 && now.hour() <= 14 ) {
        logging_msg("Irrigation not allowed between 10AM to 4:59PM. :"+String(now.hour()),true);  
        logging_msg("Stopping irrigation if it active in channel: "+String(relaynum),true);
        stop_iopin(relaynum);
      }
      else {
        logging_msg("Soil auto irrigation Started in channel: "+String(relaynum),true);  
        start_iopin(relaynum);
      }
    }
    if ( soil_moisture_p_l  >= soil_moisture_h_l ){ 
      logging_msg("Soil auto irrigation Stopped: "+String(relaynum),true);     
       stop_iopin(relaynum);
    }
      if(driprelaystatus) {
      logging_msg("Soil Drip irrigation is going on channel :"+String(relaynum),true);
      }
      if (motorrelaystatus){
       // Serial.println("Soil irrigation is stopped");
        logging_msg("Soil Sprinkle irrigation is going on channel :"+String(relaynum),true);;
      }
    
  }
  else {
    Serial.println("Soil Irrigation is not enabled for "+irr_type);
    logging_msg("Soil Irrigation is not enabled for "+irr_type,true);
  }
}

void send_soil_sensor_data(int soil_p_l){
  String datahost = String(configuration.soilHost);
  //WebSerial.print("Soil Data processing Host :");
  //WebSerial.println(datahost);
  logging_msg("Soil data host url is :"+datahost,true);
  if (datahost == "localhost"){
    soil_sensor_auto_irrigation(soil_p_l,host,driprelay);
  }
  else {
   // if(WiFi.status()== WL_CONNECTED){
    WiFiClient client;
    HTTPClient http;
    http.begin(client,datahost);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    //int httpCode = http.POST("soil_moisture_l_l="+String(soil_l_l)+"&soil_moisture_h_l="+String(soil_h_l)+"&soil_moisture_p_l="+String(soil_p_l));
    //int httpCode = http.POST("soil_moisture_p_l="+String(soil_p_l));
    int httpCode = http.POST("soil_moisture_p_l="+String(soil_p_l)+"&host="+host+"&relaychannel="+driprelay);
    String payload = http.getString();
    logging_msg(String(httpCode),true);
    logging_msg(String(payload),true);
    http.end();
    //}
    //else {
      //  WebSerial.println("wifi not connected to post soil data");
  //}

  }
  
}

int soil_sensor_read_new(){
  int isoilSensorenabled = read_eeprom(isoilSensorenabled_addr);
  int returnVal = -1;
  if ( isoilSensorenabled) {
    //digitalWrite( soil_sensor_pin, HIGH );
    //delay(10);  // Wait 10 milliseconds for sensor to settle.
    float moisture_percentage;
    moisture_percentage = ( 100.00 - ( (analogRead(soil_sensor_pin)/1023.00) * 100.00 ) );
    //digitalWrite( soil_sensor_pin, LOW );
    returnVal = round(moisture_percentage); 
  }
  else {
    logging_msg("Soil sensor not enabled",true);
  }
  return returnVal;
}

void check_soil_sensor_data(){
    int isoilSensorenabled = read_eeprom(isoilSensorenabled_addr);
    if ( isoilSensorenabled) {
     // digitalWrite( soil_sensor_pin, HIGH );
      //delay(10);    // Wait 10 milliseconds for sensor to settle.
      int ADCValue = analogRead( soil_sensor_pin );
      //digitalWrite( soil_sensor_pin, LOW );
      logging_msg("Soil Sensor analog value :"+String(ADCValue),true);
      send_soil_sensor_data(ADCValue);
    }
    else {
      logging_msg("Soil Sensor disabled in this host",true);
    }
}

void monitor_soil_auto_irrigation_data(){
  //logging_msg("inside monitor_soil_auto_irrigation_data",true);
  //logging_msg("is auto Irrigation enabled "+String(dripautoirrstatus),true);
  int sprautoirrstatus = read_eeprom(sprautoIrrStatus_addr);
  int dripautoirrstatus = read_eeprom(dripautoIrrStatus_addr);
  unsigned long soilcurrentMillis = millis();
  if(dripautoirrstatus == 1 ) {
 
     //logging_msg("soilcurrentMillis|soildatainterval|SoilautoSwitchWaithours:"+String(soilcurrentMillis)+"|"+String(soildatainterval)+"|"+String(SoilautoSwitchWaithours),true);
     Serial.println("soilcurrentMillis|soildatainterval|SoilautoSwitchWaithours:"+String(soilcurrentMillis)+"|"+String(soildatainterval)+"|"+String(SoilautoSwitchWaithours));
   // Dripp Irrigation relay start
  if(soilcurrentMillis - soildatapreMilDrip  > soildatainterval && soilcurrentMillis - soildatapreMilDrip  < SoilautoSwitchWaithours ) {
    logging_msg("Soil auto Drip Irrigation data was not processed for 35 sec",true);
    logging_msg("Stopping drip line if it active",true);
    Serial.println("Soil auto Drip Irrigation data was not processed for 35 sec");
    Serial.println("Stopping drip line if it active");
    stop_iopin(driprelay);
    driprelaystatus  = false;
  }
  if ( soilcurrentMillis - soildatapreMilDrip  > SoilautoSwitchWaithours ) {
    logging_msg("Soil auto Drip Irrigation data was not processed for more than 3 hours",true);
    logging_msg("Switching back toScheduled Drip Irrigation",true);
    Serial.println("Soil auto Drip Irrigation data was not processed for more than 3 hours");
    Serial.println("Switching back toScheduled Drip Irrigation");
    m_irr_status = 1;
    e_irr_status = 1;
  }
  else if (soilcurrentMillis - soildatapreMilDrip  <= soildatainterval){
    m_irr_status = 0;
    e_irr_status = 0;
  }
  }

    else{
    logging_msg("Soil Auto Drip Irrigation is not enabled",true);
    
  }
  //Drip Irrigation relay end

  if(sprautoirrstatus == 1 ) {
  //Sprinkle Irrigation relay start
  if(soilcurrentMillis - soildatapreMilSprink  > soildatainterval && soilcurrentMillis - soildatapreMilSprink  < SoilautoSwitchWaithours ) {
    logging_msg("Soil auto Sprinkle Irrigation data was not processed for 35 sec",true);
    logging_msg("Stopping drip line if it active",true);
    Serial.println("Soil auto Sprinkle Irrigation data was not processed for 35 sec");
    Serial.println("Stopping Sprinkle  line if it active");    
    stop_iopin(spr_motor_relay);
    motorrelaystatus = false;
  }
  if ( soilcurrentMillis - soildatapreMilSprink  > SoilautoSwitchWaithours ) {
    logging_msg("Soil auto Sprinkle Irrigation data was not processed for more than 3 hours",true);
    logging_msg("Switching back to Scheduled Sprinkle Irrigation",true);
    Serial.println("Soil auto Sprinkle Irrigation data was not processed for more than 3 hours");
    Serial.println("Switching back to Scheduled Sprinkle  Irrigation");    
    spr_m_irr_status = 1;
    spr_e_irr_status = 1;
  }
  else if (soilcurrentMillis - soildatapreMilSprink  <= soildatainterval){
    spr_m_irr_status = 0;
    spr_e_irr_status = 0;
  }
  //Sprinkle Irrigation relay end  

  }

  else{
    logging_msg("Soil Auto Sprinkle Irrigation is not enabled",true);
    
  }
 
}

void initWiFi() {
  Serial.println("Configuring wifi network...");
  Serial.println("Wifi Mode from Eeprom..."+String(configuration.isAP));
  if (configuration.isAP) {      
      WifiAPsetup();
     } 
  else {
    WifiRouterAccess();
  }

}
void WifiRouterAccess( ){
    WiFi.mode(WIFI_STA);
    Serial.println("Connecting Wifi Router");
    Serial.println("wifi ssid name :"+String(configuration.wifissid));
    WiFi.begin(configuration.wifissid, configuration.wifipass);
    delay(1000);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Still connecting wifi");
      Serial.println("wifi ssid name :"+String(configuration.wifissid));
      Serial.println("Number of retries will break after(5) :"+String(i));
      if (i > 10){
        break;
      }
      ++i;
    }
    if (WiFi.status() == WL_CONNECTED ){
      Serial.println("WiFi connected");
      Serial.print("IP address:\t");
      Serial.println(WiFi.localIP()); 
      wifiAPstatus = false;
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
    }

    else {
      Serial.println("Error: Unable to connect to Wifi Router");
      Serial.println("Switching back  Wifi mode to AP");
      WifiAPsetup();
    }
    Serial.print("Wifi mode is :");
    Serial.println(WiFi.getMode());
}
void WifiAPsetup(){
       String confapssid = String(configuration.apssid);
       String _apssid;
       String _appass;

       if (confapssid.isEmpty()) {
        _apssid = apssid;
        _appass = appass;
        //boolean result = WiFi.softAP(apssid, appass);
       }
       else{
        _apssid = configuration.apssid;
        _appass = configuration.appass;
       }
       Serial.println("Configuring wifi access point :"+_apssid);
       Serial.println("Configuring wifi access point :"+_appass);
       Serial.print("Configuring access point...");
       if (!wifiAPstatus){
          WiFi.mode(WIFI_AP);
          wifiAPstatus = WiFi.softAP(_apssid, _appass);
       }
       else {
          Serial.print("Wifi is already running in AP mode");
       }
       IPAddress apip = WiFi.softAPIP();
       Serial.print("visit: \n");
       Serial.println(apip);
       delay(1000);
    if(wifiAPstatus) {
        Serial.println("Ready");
      }
     else {
        Serial.println("Failed!");
      }
    Serial.print("Wifi mode is :");
    Serial.println(WiFi.getMode());
}
void WifiStatus()
{
    //logging_msg("RRSI: ",false);
    //logging_msg(String(WiFi.RSSI()),true);
    //logging_msg("Wifi mode is :",false);
    //logging_msg(String(WiFi.getMode()),true);
    Serial.printf("Connection status: %d\n", WiFi.status());
    Serial.print("RRSI: ");
    Serial.println(WiFi.RSSI());
    Serial.print("Wifi mode is :");
    Serial.println(WiFi.getMode()); 
  unsigned long currentMillis = millis();
  if (currentMillis - wifiStatuspreviousMillis >= wifiStatusinterval){
    switch (WiFi.status()){
      case WL_NO_SSID_AVAIL:
          /*
           * if there is no signal coverage
           */
          Serial.println("Configured SSID cannot be reached");
          WifiAPsetup();
          break;
      case WL_CONNECTED:
          Serial.println("Connection successfully established");
          Serial.print("IP address:\t");
          Serial.println(WiFi.localIP());
          break;
      case WL_CONNECT_FAILED:
          Serial.println("Connection failed");
          break;
      case WL_DISCONNECTED:
          Serial.println("Wifi Disconnected");
          break;
    }   
    wifiStatuspreviousMillis = currentMillis;
    if( !configuration.isAP  &&  WiFi.getMode() == 2) {
      Serial.println("Switching back from AP to Wifi station");
      initWiFi();
    }
  } 
}
