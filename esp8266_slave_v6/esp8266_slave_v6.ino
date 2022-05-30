#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>

#ifndef STASSID
#define STASSID "sprinkle7"
#define STAPSK  "tanulaksha321"
#endif

#define SENSOR_POWER 5
#define SOIL_PIN A0 
#define SLEEP_TIME_SECONDS 30

int numMeasure = 5;   
int ADCValue = 0;  

int relaychannel = 14;
String host = "terrace";


const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

unsigned long interval = 5000; // 5 seconds
unsigned long previousMillis = 0;
int clt_update_code;

const int led = 13;

void handleRoot() {
  String soilvalue = String( readSoil( numMeasure ) ); 
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp8266! and soil value is:"+soilvalue);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}

void setup(void) {
  //pinMode(led, OUTPUT);
  // Enable pin for Soil sensor powering
  pinMode( SENSOR_POWER , OUTPUT );
  digitalWrite( SENSOR_POWER , LOW );
  pinMode(A0, INPUT);
  //digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

   
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/gif", []() {
    static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP  started");
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
}

void loop(void) {
 // String soilvalue = String( readSoil( numMeasure ) );
 //Serial.println(soilvalue);
 // server.handleClient();
 // MDNS.update();
 // senddata();
 // ArduinoOTA.handle();  
 // delay(5000);
 clt_update_code = check_client_update();
 Serial.println("Client Update http code :"+String(clt_update_code));
 if ( clt_update_code == 200 ) {
       main_prog();
        Serial.println("Client going to deep sleep mode");
        ESP.deepSleep(SLEEP_TIME_SECONDS * 1000000);
    }
 
  else if ( clt_update_code == 202) { 
     long currentMillis = millis();
     if (currentMillis - previousMillis > interval) {
        Serial.println("client loop mode mills");
        previousMillis = currentMillis;
        main_prog();
        //delay(5000);
    }
    else {
        delay(5000);
    }
  }
  
}
void senddata(){
  WiFiClient client;
  HTTPClient http;
  String soilvalue = String( readSoil( numMeasure ) ); 
  String datahost = "http://192.168.4.1/soilsensor";
  Serial.println(soilvalue);
  http.begin(client,datahost);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST("soil_moisture_p_l="+soilvalue+"&host="+host+"&relaychannel="+relaychannel);
  String payload = http.getString();
  Serial.println(String(httpCode));
  Serial.println(String(payload));
}
int check_client_update(){
  HTTPClient http;
  WiFiClient client;
  String serverhost = "http://192.168.4.1/clientupdate";
  http.begin(client,serverhost);
  int httpCode = http.POST("");
  http.end();
  return httpCode;
}
long readSoil(int numAve) {
  long ADCValue = 0;
  for ( int i = 0; i < numAve; i++ ) {
    digitalWrite( SENSOR_POWER, HIGH );
    delay(10);    // Wait 10 milliseconds for sensor to settle.
    ADCValue += analogRead( SOIL_PIN );
    digitalWrite( SENSOR_POWER, LOW );
  }
  ADCValue = ADCValue / numAve;
  return ADCValue; 
}
void main_prog() {
  String soilvalue = String( readSoil( numMeasure ) );
  Serial.println(soilvalue);
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  senddata();  
}
