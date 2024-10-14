#include <Arduino.h>
#include <WiFi.h>
// Async Server, for Config and Settings
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// To read Data from Filesystem
#include "LittleFS.h"
#include <ArduinoJson.h>
// mdns for .local url
#include <ESPmDNS.h>
// for ntp server and time
#include "time.h"

//Reset on Pin G21
const int reset_button = 21;

const char* pre_ssid = "ClockWifi";
const char* pre_pass = "12345678";
//Simulate the logic:
const char* frontplate = "ESKISTLFÜNFZEHNZWANZIGDREIVIERTELTGNACHVORJMHALBQZWÖLFPZWEINSIEBENKDREIRHFÜNFELFNEUNVIERWACHTZEHNRSBSECHSFMUHR";
void drawDisplay(int hour, int minute);

//Global for the moment, might change later
String ssid = "";
String pass = "";
int wifi_status = WL_IDLE_STATUS;
IPAddress google_server(74,125,115,105); //to test
WiFiClient client;
AsyncWebServer server(80);

enum PageMode {SETUP, SETTINGS};
PageMode displayMode = SETUP;

//Generates a String to dynamically add the Wifi-Networks on site.
String generateWifiOptions();
// File Writing
void fileWriteData(String data, String filename);
String readFileToString(String filename);

void resetConfig();

// Currently returns 0 if something went wrong
int connectToWifi(String ssid, String pass);

// NTP stuff
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;
struct tm timeinfo;

// Minute Accurate Time:
int previousMinute = -1;
void onMinuteChange(struct tm timeinfo);
void setTimezone(String timezone);

void setup() {
  pinMode(reset_button, INPUT);
  int button_state = digitalRead(reset_button);
  // put your setup code here, to run once:
  Serial.begin(115200);

  // File System
  if(!LittleFS.begin()) {
    Serial.println("ERROR: Could not mount Filesystem");
    return;
  }

  if(button_state == HIGH){
    Serial.println("Button Pressed, Resetting");
    resetConfig();
  } else {
    Serial.println("Button't");
  }

  File file = LittleFS.open("/config.json", FILE_READ);
  if(!file){
    Serial.println("Failed to open file");
    return;
  }
  
  // Read the file into String, then close, open only if needed
  String file_content = file.readString();
  Serial.println(file_content);
  file.close();

  //Json
  JsonDocument config;
  deserializeJson(config, file_content);
  const char* j_ssid = config["ssid"];
  const char* j_pass = config["pass"];
  Serial.print("ID: ");
  Serial.print(j_ssid);
  Serial.print(" PW: ");
  Serial.println(j_pass);
  
  // Check if theres a Wifi configured already. if not, start in AP mode.
  // In addition add a variable that allows to redirect to the setup or settings page.
  if(j_ssid[0] == '\0' && j_pass[0] == '\0'){
    WiFi.softAP(pre_ssid, pre_pass);
    IPAddress ip_addr = WiFi.softAPIP();
    Serial.print("Access Point IP: ");
    Serial.println(ip_addr);
  } else {
    Serial.println("Trying to connect to Network");
    connectToWifi(j_ssid, j_pass);
    displayMode = SETTINGS;
  }
  

  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(displayMode == SETUP){
      request -> redirect("/setup");
    } else {
      request -> send (200, "text/plain", "Todo");
    }
  });

  server.on("/setup", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String config_html = readFileToString("/wifi_config.html");
    config_html.replace("%WIFI_PLACEHOLDER%", generateWifiOptions());
    request -> send(200, "text/html", config_html);
  });

  server.on("/data", HTTP_POST, [config, j_ssid, j_pass] (AsyncWebServerRequest *request) {
    if(request -> hasParam("fssid", true)) {
      ssid = request -> getParam("fssid", true) -> value();
    }
    if(request -> hasParam("fpass", true)) {
      pass = request -> getParam("fpass", true) -> value();
    }
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + pass);

    //try to connect and if successfull write to json
    if(connectToWifi(ssid, pass)) {
      String output;
      JsonDocument new_config;
      new_config["ssid"] = ssid;
      new_config["pass"] = pass;
      new_config.shrinkToFit();
      serializeJson(new_config, output);
      Serial.println(output);
      fileWriteData(output, "/config.json");
      displayMode = SETTINGS;
      request -> redirect("/");
    } else {
      request -> send (500, "text/plain", "Something went wrong, connect Arduino to PC and check debug");
    }
  });
  MDNS.begin("wordclock");
  server.begin();


  // Set Up NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
}

void loop() {
  // put your main code here, to run repeatedly:
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  int currentMinute = timeinfo.tm_min;
  if(currentMinute != previousMinute){
    onMinuteChange(timeinfo);
    previousMinute = currentMinute;
  }
}


void fileWriteData(String data, String filename){
  File file = LittleFS.open(filename, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file");
    return;
  }
  file.print(data);
  file.close();
}



String generateWifiOptions() {
  String options = "";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    options += "<option>No networks found</option>";
  } else {
    for (int i = 0; i < n; i++) {
      options += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>";
    }
  }
  return options;
}

int connectToWifi(String ssid, String pass) {
  Serial.print("Attempting to connect to: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());
  //WiFi.begin(ssid, pass);
  int retries = 0;
  const long interval = 500;
  unsigned long prev_millis = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    unsigned long current_millis = millis();
    if(current_millis - prev_millis >= interval){
      prev_millis = current_millis;
      Serial.print(".");
      retries++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return 1;
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    return 0;
  }
}

void resetConfig(){
  String output;
  JsonDocument new_config;
  new_config["ssid"] = "";
  new_config["pass"] = "";
  new_config.shrinkToFit();
  serializeJson(new_config, output);
  Serial.println(output);
  fileWriteData(output, "/config.json");
}

void onMinuteChange(struct tm timeinfo){
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M");
  int hour = timeinfo.tm_hour;
  int min = timeinfo.tm_min;
  drawDisplay(hour, min);
}

void setTimezone(String timezone){
  Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

String readFileToString(String filename){
  String file_content;
  File file = LittleFS.open(filename, FILE_READ);
  if(!file){
    Serial.println("Failed to open file");
  } else {
      file_content = file.readString();
    //Serial.println(file_content);
    file.close();
  }
  return file_content;
}

void drawDisplay(int hour, int minute){
  
}