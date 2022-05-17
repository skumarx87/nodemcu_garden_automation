/*
 * V5 Adding Soil Moisture sensor
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

AsyncWebServer server(80);

extern "C" {
#include<user_interface.h>
}

RTC_DS3231 rtc;

#define LED 2
const int soil_sensor_pin = A0;

//String current_time;

// Replace with your network credentials
const char* ssid = "sprinkle";
const char* password = "tanulaksha321"; 

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

const int soilautoirrstatus_addr = 31;
const int soil_lowerlevel_addr = 32;
const int soil_upperlevel_addr = 33;
const int isoilSensorenabled_addr = 34;

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

bool soilautoirrstatus = true;
bool isoilSensorenabled = true;

long soildatapreviousMillis = 0;
long soildatainterval = 10000; 

template <class T> int EEPROM_writeAnything(int ee, const T& value);
template <class T> int EEPROM_readAnything(int ee, T& value);

struct wifiConfig 
{
  boolean isAP = false;
  char wifissid[30];
  char pass[30];
  char soilHost[40] = "localhost";
} configuration;

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  EEPROM.begin(512);
  readConfigFromEEPROM();
  pinMode(driprelay, OUTPUT);
  pinMode(spr_motor_relay, OUTPUT);
  digitalWrite(driprelay, HIGH);
  digitalWrite(spr_motor_relay, HIGH);
  driprelaystatus=false;
  motorrelaystatus = false;
  rtc.begin();
  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  Serial.println("Configuring wifi network...");
  Serial.println("Wifi Mode from Eeprom..."+String(configuration.isAP));
  if (configuration.isAP) {
  Serial.print("Configuring access point...");
  WiFi.mode(WIFI_AP);
  boolean result = WiFi.softAP(ssid, password);
  if(result) {
    Serial.println("Ready");
  }
  else {
    Serial.println("Failed!");
  }
  }
  else {
    Serial.println("Connecting Wifi Router");
    Serial.println("wifi ssid name :"+String(configuration.wifissid));
    Serial.println("wifi ssid pass :"+String(configuration.pass));
    WiFi.begin(configuration.wifissid, configuration.pass);
    delay(1000);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Still connecting wifi");
      Serial.println("wifi ssid name :"+String(configuration.wifissid));
      Serial.println("Number of retries will break after(5) :"+String(i));
      if (i > 5){
        break;
      }
      ++i;
    }
    if (WiFi.status() == WL_CONNECTED ){
      Serial.println("WiFi connected");
      Serial.print("IP address:\t");
      Serial.println(WiFi.localIP()); 
    }

    else {
      Serial.println("Error: Unable to connect to Wifi Router");
      Serial.println("changing Wifi mode to AP");
      configuration.isAP = true;
      saveConfigToEEPROM();
      ESP.reset();
    }

  }
  delay(100);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("OFF hit.");
      int params = request->params();
      Serial.printf("%d params sent in\n", params);
    if ( request->hasParam(i_s_s_k) && request->hasParam(i_f_a_k)) {
      i_s_s_v= request->getParam(i_s_s_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_s_s_v == "mor"  &&  i_f_a_v == "on") {
        update_eeprom(m_on_s_addr,1);
      }
      else if ( i_s_s_v == "mor"  &&  i_f_a_v == "off") {
        update_eeprom(m_on_s_addr,0);
        }
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "on") {
        update_eeprom(e_on_s_addr,1);
        }  
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "off") {
        update_eeprom(e_on_s_addr,0);
        }   
        request->redirect("/");           
    }
    if ( request->hasParam(s_i_s_s_k) && request->hasParam(i_f_a_k)) {
      i_s_s_v= request->getParam(s_i_s_s_k)->value();
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_s_s_v == "mor"  &&  i_f_a_v == "on") {
        update_eeprom(spr_m_on_s_addr,1);
      }
      else if ( i_s_s_v == "mor"  &&  i_f_a_v == "off") {
        update_eeprom(spr_m_on_s_addr,0);
        }
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "on") {
        update_eeprom(spr_e_on_s_addr,1);
        }  
      else if ( i_s_s_v == "eve"  &&  i_f_a_v == "off") {
        update_eeprom(spr_e_on_s_addr,0);
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
        start_iopin(driprelay,driprelaystatus);
        logging_msg("Drip Line Ondemand water started: ",true);
        //digitalWrite(driprelay, LOW);
        //driprelaystatus = true;
      }
      else if ( i_f_a_v == "off" ){
        stop_iopin(driprelay,driprelaystatus);
        logging_msg("Drip Line Ondemand water Stopped: ",true);
        //digitalWrite(driprelay, HIGH);
        //driprelaystatus = false;
      }
      request->redirect("/");
                  
    }    
    if ( request->hasParam(i_spr_on_d_w_k) && request->hasParam(i_f_a_k)) {
      i_f_a_v = request->getParam(i_f_a_k)->value();
      if ( i_f_a_v == "on" ){
        start_iopin(spr_motor_relay,motorrelaystatus);
        //digitalWrite(spr_motor_relay,LOW);
        //motorrelaystatus = true;
        logging_msg("Sprinkle Ondemand water started: ",true);
        
      }
      else if ( i_f_a_v == "off" ){
        stop_iopin(spr_motor_relay,motorrelaystatus);
        //digitalWrite(spr_motor_relay,HIGH);
        //motorrelaystatus = false;
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
        String wifimode = request->getParam("wifimode",true)->value();  
        Serial.println("Post Method Received ssid : "+wifiname);
        Serial.println("Post Method Received Wifi Mode : "+wifimode);
        if (wifimode == "on" ){
        configuration.isAP = true;
        }
        else {
          configuration.isAP = false;
        }
        wifiname.toCharArray(configuration.wifissid,30);
        pass_name.toCharArray(configuration.pass,30);
        saveConfigToEEPROM();
        ESP.reset();
    }    
   });
   server.on("/soilsensor", HTTP_POST, [](AsyncWebServerRequest *request){
    String soil_moisture_l_l = request->getParam("soil_moisture_l_l",true)->value();
    String soil_moisture_h_l = request->getParam("soil_moisture_h_l",true)->value();
    String soil_moisture_p_l = request->getParam("soil_moisture_p_l",true)->value();    
    soil_sensor_auto_irrigation(soil_moisture_l_l.toInt(),soil_moisture_h_l.toInt(),soil_moisture_p_l.toInt());
    request->send(200);
   });
   WebSerial.begin(&server);
   WebSerial.msgCallback(recvMsg);
   server.begin();
}
void loop(){
  drip_irrigation();
  sprinkle_irrigation();
  client_status();
  soil_sensor_read();
  monitor_soil_auto_irrigation_data();
  delay(1000);
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
 int m_checked=read_eeprom(m_on_s_addr);
 int e_checked=read_eeprom(e_on_s_addr);
 int m_s_h = read_eeprom(m_on_h_addr);
 int m_s_m = read_eeprom(m_on_m_addr);
 int m_duration = read_eeprom(m_on_d_addr);
 int e_s_h = read_eeprom(e_on_h_addr);
 int e_s_m = read_eeprom(e_on_m_addr);
 int e_duration = read_eeprom(e_on_d_addr);
    
 int spr_m_checked=read_eeprom(spr_m_on_s_addr);
 int spr_e_checked=read_eeprom(spr_e_on_s_addr);
 int spr_m_s_h = read_eeprom(spr_m_on_h_addr);
 int spr_m_s_m = read_eeprom(spr_m_on_m_addr);
 int spr_m_duration = read_eeprom(spr_m_on_d_addr);
 int spr_e_s_h = read_eeprom(spr_e_on_h_addr);
 int spr_e_s_m = read_eeprom(spr_e_on_m_addr);
 int spr_e_duration = read_eeprom(spr_e_on_d_addr);

 int soil_lowerlevel = read_eeprom(soil_lowerlevel_addr);
 int soil_upperlevel = read_eeprom(soil_upperlevel_addr);
 int soilautoirrstatus = read_eeprom(soilautoirrstatus_addr);
 int isoilSensorenabled = read_eeprom(isoilSensorenabled_addr);
 
if(var == "M_ON_CHECKED"){
    if (m_checked == 1 ){
      return "checked";
    }
    else if ( m_checked == 0 ){
      return "";
    }
  }
  else if (var == "M_OFF_CHECKED"){
    if ( m_checked == 0 ){
      return "checked";
    }
    else if ( m_checked == 1 ){
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
    if ( e_checked == 1 ){
      return "checked";
    }
    else if ( e_checked == 0 ){
      return "";
    }
  }
  else if ( var == "E_OFF_CHECKED"){
    if ( e_checked == 0 ){
      return "checked";
    }
    else if ( e_checked == 1 ){
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
    if (spr_m_checked == 1 ){
      return "checked";
    }
    else if ( spr_m_checked == 0 ){
      return "";
    }
  }
  else if (var == "SPR_M_OFF_CHECKED"){
    if ( spr_m_checked == 0 ){
      return "checked";
    }
    else if ( m_checked == 1 ){
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
    if ( spr_e_checked == 1 ){
      return "checked";
    }
    else if ( spr_e_checked == 0 ){
      return "";
    }
  }
  else if ( var == "SPR_E_OFF_CHECKED"){
    if ( spr_e_checked == 0 ){
      return "checked";
    }
    else if ( spr_e_checked == 1 ){
      return "";
    }
  }
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
    return configuration.wifissid;
  }  
  if (var == "WIFI_PASS") {
    return configuration.pass;
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
  if (var == "SOIL_AUTO_ON_CHECKED") {
    if (soilautoirrstatus) {
      return "checked";
    }
    else {
      return "";
    }
  }  
  if (var == "SOIL_AUTO_OFF_CHECKED") {
    if (soilautoirrstatus) {
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
  //logging_msg(String(address),true);
  //logging_msg("Update Value :",false);
  //logging_msg(String(value),true);
  //Serial.println("Restarting Nodemcu");
  //ESP.restart();
}
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

void drip_irrigation() {
  irrigation_on(driprelay, driprelaystatus, m_on_h_addr, m_on_m_addr, e_on_h_addr, e_on_m_addr, m_on_d_addr, e_on_d_addr, m_on_s_addr, e_on_s_addr,drip);
}
void sprinkle_irrigation() {
  irrigation_on(spr_motor_relay, motorrelaystatus, spr_m_on_h_addr, spr_m_on_m_addr, spr_e_on_h_addr, spr_e_on_m_addr, spr_m_on_d_addr, spr_e_on_d_addr, spr_m_on_s_addr, spr_e_on_s_addr,sprinkle);
}

void irrigation_on(int relayNum,bool &relaystatus,int m_h_add, int m_m_add,int e_h_add,int e_m_add,int m_d_add,int e_d_add,int m_s_add, int e_s_add,String msg){
  int m_irr_status=read_eeprom(m_s_add);
  int e_irr_status=read_eeprom(e_s_add);
  drip_sch_arry = getSchedule(m_h_add, m_m_add, e_h_add, e_m_add, m_d_add, e_d_add );
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
      start_iopin(relayNum,relaystatus);
      logging_msg(msg+": Morning Irrigation Started: ",true);
      logging_msg(msg+": Morning Irrigation Endtime: "+String(mOffHour)+":"+String(mOffMin),true);
      //relaystatus = true;
    }
    else if(now.hour() == mOffHour && now.minute() == mOffMin) {
      stop_iopin(relayNum,relaystatus);
      //digitalWrite(relayNum, HIGH);
      logging_msg(msg+": Morining Irrigation stopped",true);
     //relaystatus = false;
    }
  }
  else {
    banner_msg(msg+": Morning irrigation off");
  }
  if ( e_irr_status == 1 ) {
    if(now.hour() == eOnHour && now.minute() == eOnMin){
     //digitalWrite(relayNum, LOW);
      start_iopin(relayNum,relaystatus);
      logging_msg(msg+": Evening Irrigation Started: ",true);
      logging_msg(msg+": Evening Irrigation Endtime: "+String(eOffHour)+":"+String(eOffMin),true);
      //relaystatus = true;
    }
    else if(now.hour() == eOffHour && now.minute() == eOffMin) {
      //digitalWrite(relayNum, HIGH);
      stop_iopin(relayNum,relaystatus);
      logging_msg(msg+": Evening Irrigation stopped",true);
      //relaystatus = false;
    }
  }
  else {
    banner_msg(msg+" Evening irrigation off");
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

void start_iopin(int pin,bool &iopin_status) {
  digitalWrite(pin, LOW);
  iopin_status = true;
}
void stop_iopin(int pin,bool &iopin_status){
  digitalWrite(pin, HIGH);
  iopin_status = false;
}
void banner_msg( String msg) {
  if (debug_msg) {
    DateTime now = rtc.now();
    char buf2[] = "DD/MM/YY-hh:mm:ss";
    WebSerial.println(String(now.toString(buf2))+":"+msg);
  }
}
void logging_msg( String msg,bool newline) {
    DateTime now = rtc.now();
    char buf2[] = "DD/MM/YY-hh:mm:ss";
    if (newline) {
      WebSerial.println(String(now.toString(buf2))+":"+msg);
    }
    else {
      WebSerial.print(String(now.toString(buf2))+":"+msg);
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
  ESP.reset();
}

void soil_sensor_auto_irrigation(int soil_moisture_l_l,int soil_moisture_h_l,int soil_moisture_p_l) {
  if(soilautoirrstatus) {
    unsigned long currentMillis = millis();
    soildatapreviousMillis = currentMillis;
    Serial.println(soil_moisture_l_l);
    Serial.print(soil_moisture_h_l);
    Serial.println(soil_moisture_p_l);
    if ( soil_moisture_p_l  <= soil_moisture_l_l ){
      Serial.print("Started auto irrigation");
    //start_iopin(driprelay,driprelaystatus);
    }
    if ( soil_moisture_p_l  >= soil_moisture_h_l ){
      Serial.print("Stopping auto irrigation");
      //stop_iopin(driprelay,driprelaystatus);
    }
    
  }
}
void send_soil_sensor_data(int soil_l_l,int soil_h_l,int soil_p_l){
  
  String datahost = String(configuration.soilHost);
  Serial.print("Soil data host url is :"+datahost);
  if (datahost == "localhost"){
    Serial.print("Soil data will be posted to local server");
    soil_sensor_auto_irrigation(soil_l_l,soil_h_l,soil_p_l);
  }
  else {
    if(WiFi.status()== WL_CONNECTED){
    WiFiClient client;
    HTTPClient http;
    http.begin(client,datahost);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST("soil_moisture_l_l=40&soil_moisture_h_l=80&soil_moisture_p_l=40");
    String payload = http.getString();
    Serial.println(httpCode);
    Serial.println(payload);
    http.end();
    }
    else {
        Serial.println("wifi not connected");
  }

  }
  
}
void soil_sensor_read(){
  int isoilSensorenabled = read_eeprom(isoilSensorenabled_addr);
  if ( isoilSensorenabled) {
    //int soil_lowerlevel = read_eeprom(soil_lowerlevel_addr);
    //int soil_upperlevel = read_eeprom(soil_upperlevel_addr);

    int soil_lowerlevel = 50;
    int soil_upperlevel = 80;
    int present_level = 30;
    
    float moisture_percentage;
    moisture_percentage = ( 100.00 - ( (analogRead(soil_sensor_pin)/1023.00) * 100.00 ) );
    logging_msg("Soil Moisture(in Percentage) = ",false);
    logging_msg(String(moisture_percentage),false);
    logging_msg("%",true);
    logging_msg(String(analogRead(soil_sensor_pin)),true);
    send_soil_sensor_data(soil_lowerlevel,soil_upperlevel,present_level);
  }
  else{
    logging_msg("Soil sensor not enabled",true);
  }
}
void monitor_soil_auto_irrigation_data(){
  if(soilautoirrstatus) {
  unsigned long currentMillis = millis();
  if(currentMillis - soildatapreviousMillis > soildatainterval) {
    Serial.println("Soil auto Irrigation data was not processed for 10 sec");
    Serial.println("Stopping drip line if it active");
    //stop_iopin(driprelay,driprelaystatus);
  }
  }
}
