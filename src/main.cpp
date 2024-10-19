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
// LEDs
#include <Adafruit_NeoPixel.h>

//Reset on Pin G21
const int reset_button = 21;

const char* pre_ssid = "ClockWifi";
const char* pre_pass = "12345678";
//Simulate the logic:
const char* frontplate = "ESKISTLF\xF5NFZEHNZWANZIGDREIVIERTELTGNACHVORJMHALBQZW\xEFLFPZWEINSIEBENKDREIRHF\xF5NFELFNEUNVIERWACHTZEHNRSBSECHSFMUHR";

enum wordtype {S_IT, S_IS, M_FIVE, M_TEN, M_TWENTY, M_FIFTEEN, M_FORTYFIVE, S_TO, S_PAST, M_HALF, S_CLOCK};
const int words[][2] = {{0,1},{3,5},{7,10},{11,14},{15,21},{26,32},{22,25},{39,41},{35,38},{44,47},{107,109},
                        {49,53},{57,60},{55,59},{67,70},{84,87},{73,76},{100,104},{60,65},{89,92},{80,83},{93,96},{77,79}};
const int words_offset = 11;

void drawDisplay(int hour, int minute);
void displayOutput(bool ledmatrix[]);
void simulateDisplayOutput(bool ledmatrix[], String front, int min);

//LEDs
const int led_data = 13;
const int led_num = 114;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(led_num, led_data, NEO_GRB + NEO_KHZ800);


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
  pixels.setPixelColor(0, pixels.Color(255,0,0));
  pixels.show();
  // put your setup code here, to run once:
  Serial.begin(115200);
  /*for(int i = 0; i < led_num; i++){
    pixels.setPixelColor(i, pixels.Color(65,50,35)); // Moderately bright green color.
    pixels.show(); // This sends the updated pixel color to the hardware.
  }*/
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
  pixels.setPixelColor(1, pixels.Color(255,0,0));
  pixels.show();

  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(displayMode == SETUP){
      request -> redirect("/setup");
    } else {
      //request -> send (200, "text/plain", "Todo");
      String settings_html = readFileToString("/settings.html");
      request -> send(200, "text/html", settings_html);
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

  server.on("/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("color", true)) {
        String color = request->getParam("color", true)->value();
        Serial.println("Selected color: " + color);
        String hexColor = color.substring(1);

        long rgbValue = strtol(hexColor.c_str(), NULL, 16);
        int red = (rgbValue >> 16) & 0xFF;
        int green = (rgbValue >> 8) & 0xFF;
        int blue = rgbValue & 0xFF;

        for(int i = 0; i < led_num; i++){
          pixels.setPixelColor(i, pixels.Color(red,green,blue)); // Moderately bright green color.
          pixels.show(); // This sends the updated pixel color to the hardware.
        }
    }
    request->send(200, "text/plain", "Color received");
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
  int hour = timeinfo.tm_hour%12;
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
  int min1to4 = minute%5;
  int min5 = minute - min1to4;
  int hour12h = hour;
  if(minute >= 25){
    hour12h = (hour + 1)%12;
  }
  bool matrix[114];
  memset(matrix, 0, sizeof(bool)*114);
  // Static Stuff
  for(int i = 0; i < 2; i++){
    for(int j = words[i][0]; j<= words[i][1]; j++){
      matrix[j] = 1;
    }
  }
  // Minute
  if(min5 < 4) {
    for(int i = words[S_CLOCK][0]; i <= words[S_CLOCK][1]; i++){
      matrix[i] = 1;
    }
  } else if (min5 >= 25 && min5 < 40) {
    for(int i = words[M_HALF][0]; i <= words[M_HALF][1]; i++){
      matrix[i] = 1;
    }
  }
  // PAST
  if((min5 >= 5 && min5 < 25) || (min5 >= 35 &&min5 < 40)){
    for(int i = words[S_PAST][0]; i <= words[S_PAST][1]; i++){
      matrix[i] = 1;
    }
  }
  // TO
  if((min5 >= 25 && min5 < 30) || (min5 >= 40 &&min5 < 60)){
    for(int i = words[S_TO][0]; i <= words[S_TO][1]; i++){
      matrix[i] = 1;
    }
  }
  // FIVE
  if((min5 >= 5 && min5 < 10) || (min5 >= 55 && min5 < 60) || (min5 >= 25 && min5 < 30) || (min5 >= 35 && min5 < 40)){
    for(int i = words[M_FIVE][0]; i <= words[M_FIVE][1]; i++){
      matrix[i] = 1;
    }
  }
  // TEN
  if((min5 >= 10 && min5 < 15) || (min5 >= 50 && min5 < 55)){
    for(int i = words[M_TEN][0]; i <= words[M_TEN][1]; i++){
      matrix[i] = 1;
    }
  }
  // QUARTER
  if((min5 >= 15 && min5 < 20) || (min5 >= 45 && min5 < 50)){
    for(int i = words[M_FIFTEEN][0]; i <= words[M_FIFTEEN][1]; i++){
      matrix[i] = 1;
    }
  }
  // TWENTY
  if((min5 >= 20 && min5 < 25) || (min5 >= 40 && min5 < 45)){
    for(int i = words[M_TWENTY][0]; i <= words[M_TWENTY][1]; i++){
      matrix[i] = 1;
    }
  }
  for(int i = words[words_offset+hour12h][0]; i <= words[words_offset+hour12h][1]; i++){
    matrix[i] = 1;
  }
  // MINUTES
  if(min1to4 > 0)
    matrix[110] = 1;
  if(min1to4 > 1)
    matrix[111] = 1;
  if(min1to4 > 2)
    matrix[112] = 1;
  if(min1to4 > 3)
    matrix[113] = 1;
  // The Real Clock updates the LEDs here.
  simulateDisplayOutput(matrix, frontplate, min1to4);
  displayOutput(matrix);
}

void simulateDisplayOutput(bool ledmatrix[], String front, int min){
  String toPrint = "";
  if(min > 0){
    toPrint.concat("*");
  } else {
    toPrint.concat("\n ");
  }
  if(min > 1){
    toPrint.concat("           *\n ");
  } else {
    toPrint.concat("\n ");
  }
  for(int i = 0; i < 110; i++){
    if(i % 11 == 0 && i != 0){
      toPrint.concat("\n ");
    }
    if(ledmatrix[i]){
      toPrint.concat(front[i]);
    } else {
      toPrint.concat(" ");
    }
  }
    if(min > 2){
    toPrint.concat("\n*");
  }
  if(min > 3){
    toPrint.concat("           *");
  }
  Serial.println(toPrint);
}


void displayOutput(bool ledmatrix[]){
  for (int i = 0; i < led_num; i++){
    if(ledmatrix[i]){
      pixels.setPixelColor(i, pixels.Color(50,50,50));
    } else {
      pixels.setPixelColor(i, pixels.Color(0,0,0));
    }
  }
  pixels.show();
}