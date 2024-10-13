#include <Arduino.h>
#include <WiFi.h>
// Async Server, for Config and Settings
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// To read Data from Filesystem
#include "LittleFS.h"
#include <ArduinoJson.h>


//Reset on Pin G21
const int reset_button = 21;

const char wifi_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; padding: 20px; margin: 0; }
    form {
      display: inline-block;
      text-align: left;
      background-color: #f9f9f9;
      padding: 15px;
      border-radius: 5px;
      box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
      width: 100%;
      max-width: 300px;
      margin: auto;
    }
    h1 {
      font-size: 22px;
      margin-bottom: 15px;
    }
    label {
      font-size: 14px;
      margin-bottom: 5px;
      display: block;
    }
    select, input[type="password"], input[type="submit"] {
      width: 100%;
      padding: 8px;
      margin: 5px 0 10px 0;
      display: block;
      border: 1px solid #ccc;
      border-radius: 4px;
      box-sizing: border-box;
    }
    input[type="submit"] {
      background-color: #4CAF50;
      color: white;
      border: none;
      cursor: pointer;
    }
    input[type="submit"]:hover {
      background-color: #45a049;
    }
  </style>
</head>
<body>
  <h1>ESP32 Web Server</h1>
  <form action="/data" method="POST">
    <label for="fssid">Wifi Name:</label>
    <select id="fssid" name="fssid">
      %WIFI_PLACEHOLDER%
    </select>
    <label for="fpass">Password:</label>
    <input type="password" id="fpass" name="fpass">
    <input type="submit" value="Connect">
  </form>
</body>
</html>
)rawliteral";

const char* pre_ssid = "Clock Wifi";
const char* pre_pass = "12345678";

//Global for the moment, might change later
String ssid = "";
String pass = "";
int wifi_status = WL_IDLE_STATUS;
IPAddress google_server(74,125,115,105); //to test
WiFiClient client;
AsyncWebServer server(80);

//Generates a String to dynamically add the Wifi-Networks on site.
String generateWifiOptions();
// File Writing
void fileWriteData(String data, String filename);

void resetConfig();

// Currently returns 0 if something went wrong
int connectToWifi(String ssid, String pass);

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
  
  if(j_ssid[0] == '\0' && j_pass[0] == '\0'){
    WiFi.softAP(pre_ssid, pre_pass);
    IPAddress ip_addr = WiFi.softAPIP();
    Serial.print("Access Point IP: ");
    Serial.println(ip_addr);
  } else {
    Serial.println("Trying to connect to Network");
    connectToWifi(j_ssid, j_pass);
  }
  

  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String config_html = wifi_config_html;
    config_html.replace("%WIFI_PLACEHOLDER%", generateWifiOptions());
    request -> send(200, "text/html", config_html);
  });

  server.on("/data", HTTP_POST, [config, j_ssid, j_pass] (AsyncWebServerRequest *request) {
    //reset both
    //String ssid = "";
    //String pass = "";
    if(request -> hasParam("fssid", true)) {
      ssid = request -> getParam("fssid", true) -> value();
    }
    if(request -> hasParam("fpass", true)) {
      pass = request -> getParam("fpass", true) -> value();
    }
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + pass);
    request -> send (200, "text/plain", "OK");

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
    }
  });

  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
    if (WiFi.status() == WL_CONNECTED) {
    if (client.connect(google_server, 80)) {
      Serial.println("Connected to Google server");
      client.stop();
    }
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
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(500);
    Serial.print(".");
    retries++;
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