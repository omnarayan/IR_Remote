#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

#include <Arduino.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>


//json parser
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>


// wifimanager can run in a blocking mode or a non blocking mode
// Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#define TRIGGER_PIN 0
WiFiManager wm; 
WiFiManagerParameter nerve_server_host;
WiFiManagerParameter nerve_server_port;
WiFiManagerParameter license_key;

//display
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
#include "images.h"
#include "config.h"
typedef void (*Display)(void);
// Add this library: https://github.com/markszabo/IRremoteESP8266
#include <IRremoteESP8266.h>
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%



const uint16_t kRecvPin = 14;

// GPIO to use to control the IR LED circuit. Recommended: 4 (D2).
const uint16_t kIrLedPin = 12;

// The Serial connection baud rate.
// NOTE: Make sure you set your Serial Monitor to the same speed.
const uint32_t kBaudRate = 115200;

const int Led_Red = 2;
const int Led_Green = 13;
const int Led_Blue = 15;

// As this program is a special purpose capture/resender, let's use a larger
// than expected buffer so we can handle very large IR messages.
const uint16_t kCaptureBufferSize = 1024;  // 1024 == ~511 bits

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
const uint8_t kTimeout = 50;  // Milli-Seconds

// kFrequency is the modulation frequency all UNKNOWN messages will be sent at.
const uint16_t kFrequency = 38000;  // in Hz. e.g. 38kHz.



SSD1306Wire display(0x3c, SDA, SCL); // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h
// The IR transmitter.
IRsend irsend(kIrLedPin);
// The IR receiver.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, false);
// Somewhere to store the captured message.
decode_results results;

//const String ssid = "izingawireless_2.4G";
char* password = SSID_PASSWPRD;
String nerveURL =  "http://" + String(NERVE_SERVER_HOST) + ":" + String(NERVE_SERVER_PORT) + "/neuron/v3/node?licensekey=" + String(LICENSE_KEY) + "&machineID=" + WiFi.macAddress() + "&name=" + WiFi.macAddress();
String nerveServerHost = "";
String nerveServerPORT = "";
String licenseKey = "";
bool isConnectedToNerve = false;

ESP8266WebServer server(PORT);

int displayMode = 0;
int counter = 1;


long timeSinceLastModeSwitch = 0;
long timeSinceLastWifiChecked = 0;
long timeSinceLastNerveCheck = 0;


void setup(void) {

  pinMode(Led_Red, OUTPUT);
  pinMode(Led_Green, OUTPUT);
  pinMode(Led_Blue, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  irrecv.enableIRIn();  // Start up the IR receiver.
  irsend.begin();       // Start up the IR sender.
  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  digitalWrite(BUILTIN_LED, HIGH);
  Serial.begin(115200);
  setupWifi();
//  WiFi.begin(SSID, password);
  Serial.println("");

  // Wait for connection
//  connectToWifi();


  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS Responder Started");
  }

  server.on("/", heartBeat);
  server.on("/test", handleTest);
  server.on("/notifynerve", notifyNerve);
  server.on("/run", handleRun);
  server.on("/info", heartBeat);
  server.on("/v2/heartbeat", heartBeat);
  server.on("/record", handleRecord);
  server.on("/reset", resetConfig);
  
  server.onNotFound(handleNotFound);


  server.begin();
  setGreen();
  Serial.println("HTTP Server Started");
}

// setup wifi
void setupWifi(){
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  delay(3000);
  Serial.println("\n Starting");

  pinMode(TRIGGER_PIN, INPUT);
  
//   wm.resetSettings(); // wipe settings

  if(wm_nonblocking) wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(1200);
  new  (&nerve_server_host)WiFiManagerParameter("nerveServerHost", "nerve server ip", "", 40);
  wm.addParameter(&nerve_server_host);
 new  (&nerve_server_port)WiFiManagerParameter ("nerveServerPort", "nerve server port", "", 6);
  wm.addParameter(&nerve_server_port);
  new (&license_key)WiFiManagerParameter ("licenseKey", "License Key", "", 40);
  wm.addParameter(&license_key);
  wm.setSaveParamsCallback(saveParamCallback);
   bool res;
   displayWifiConfig();
   res = wm.autoConnect(AP_NAME, AP_PASSWORD);
   if(!res) {
    Serial.println("Failed to connect or hit timeout");
    ESP.restart();
  } 
  else {
    notifyNerve();
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
  }

  
}

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("nerveServerHost"));
   nerveURL =  "http://" + getParam("nerveServerHost") + ":" + getParam("nerveServerPort") + "/neuron/v3/node?licensekey=" + getParam("licenseKey") + "&machineID=" + WiFi.macAddress() + "&name=" + WiFi.macAddress();
   Serial.println("nerveURL " + nerveURL);
   
//  notifyNerve();
}

void resetConfig(){
  server.send(200, "text/json", "sent remote command");
  wm.resetSettings();
}


void connectToWifi() {
  int count = 0;
  counter = 20;
  while (WiFi.status() != WL_CONNECTED) {
    drawProgressBarForWifiConnection();
    for (int val = 255; val > 0; val--) {
      analogWrite (Led_Red, val);
      analogWrite (Led_Blue, 255 - val);
      analogWrite (Led_Green, 128 - val);
      delay (5);
      val = val - 5;
    }

    for (int val = 0; val < 255; val++) {
      analogWrite (Led_Red, val);
      analogWrite (Led_Blue, 255 - val);
      analogWrite (Led_Green, 128 - val);
      val = val + 5;
      delay (5);
    }
    if (count > 10 && count % 2 == 0) {
      displayWifiDetails();
      delay(WIFI_DISPLAY_DURATION);
    }

    count ++ ;
    Serial.print(".");
  }
  timeSinceLastWifiChecked = millis();
  notifyNerve();
}


Display displayInfoMode[] = {   displayInfo, drawIcon};
int displayModeLength = (sizeof(displayInfoMode) / sizeof(Display));

void loop(void) {
  display.clear();
   if ( digitalRead(TRIGGER_PIN) == LOW) {
   
    //reset settings - for testing
    wm.resetSettings();
  
    setupWifi();
  }
  displayInfoMode[displayMode]();
  digitalWrite(BUILTIN_LED, HIGH);
  setGreen();
  server.handleClient();
  setGreen();
  digitalWrite(BUILTIN_LED, LOW);
  if (millis() - timeSinceLastModeSwitch > INFO_DISPLAY_DURATION) {
    displayMode = (displayMode + 1)  % displayModeLength;
    timeSinceLastModeSwitch = millis();
  }
  if (millis() - timeSinceLastModeSwitch > WIFI_CHECK_DURATION) {
//    connectToWifi();
  }
  if (millis() - timeSinceLastModeSwitch > NERVE_CHECK_DURATION) {\
    notifyNerve();
//    timeSinceLastNerveCheck = millis();
  }
  display.display();
 

}

void drawIcon() {
  display.clear();
  display.drawXbm(0, 15, robusTestIconWidth, robusTestIconHeight, robusTestIconBits);
}


void displayWifiDetails() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "MAC - " + WiFi.macAddress());
  display.drawString(0, 14, SSID );
  display.drawString(0, 28, password);
  display.display();

}


void displayWifiConfig() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Connect to bellow WIFI  ");
  display.drawString(0, 14, "AP - " + String(AP_NAME ));
  display.drawString(0, 28, "Password - " + String(AP_PASSWORD));
  display.display();

}



void displayInfo() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "MAC - " + WiFi.macAddress());
  display.drawString(0, 14, SSID );
  display.drawString(0, 28, "IP - " +  WiFi.localIP().toString());
  display.drawString(0, 42, String(NERVE_SERVER_HOST) + ":" + String(NERVE_SERVER_PORT));
  display.display();

}

void drawProgressBarForWifiConnection() {
  display.clear();
  
  int progress = (counter / 5) % 100;
  // draw the progress bar
  display.drawXbm(34, 0, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.drawProgressBar(0, 40, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 50, String(progress) + "%");
  display.display();
  counter  = counter + 100;

}

void handleTest() {
  
  Serial.println("Sat Ok");
  irsend.sendNEC(0xA25DFA05, 32);

  delay(1000);
  irsend.sendSAMSUNG(3772802968, 32);
  //  irsend.sendSAMSUNG(0xE0E06798, 32);
  server.send(200, "text/plain", "Sat OK");

}


void handleRun() {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 20, "Playing");
  display.display();
  setBlue();
  if (server.hasArg("plain") == false) { //Check if body received

    server.send(200, "text/plain", "Body not received");
    return;

  }
  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(400, "text/plain", error.f_str());
    return;
  }

  decode_type_t protocol = strToDecodeType(doc["protocolName"]);
  String protocolName = typeToString(protocol).c_str();
  const char* value = doc["value"] ;
  uint16_t size = doc["size"];
  uint64_t  code = doc["code"];
  Serial.print("\nvalue - "   );
  Serial.print(value);
  Serial.print("\ncodeStr - ");

  Serial.print("\ncode - " );

  bool success = irsend.send(protocol, code, size);


  if (!success) {
    server.send(400, "text/json", "unable to run remote command");
  } else {
    server.send(200, "text/json", "sent remote command");
  }

  setGreen();
  display.clear();
}
void handleRecord() {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 0, "Recording");
  if (server.hasArg("plain") == false) { //Check if body received

    server.send(200, "text/plain", "No payload found");
    return;

  }
  StaticJsonDocument<500> payload;
  DeserializationError error = deserializeJson(payload, server.arg("plain"));

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(400, "text/plain", error.f_str());
    return;
  }
  String button = payload["buttonName"];
  String msg = payload["msg"];
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 30, msg);
  display.drawString(0, 48, button);

  display.display();
  setRed();
  decode_type_t protocol;
  String protocolName;
  String value ;
  uint16_t size ;
  uint64_t code;
  size = 0;
  while (size < 1) {
    getIRSensorData(&protocol, &protocolName, &value, &code, &size);
    yield();
  }
  Serial.print("\nvalue - "  + value);
  Serial.print("\nsize " );
  Serial.print(size );
  Serial.print("\n" );

  StaticJsonDocument<200> doc;
  //  JsonObject &root = jsonBuffer.createObject();x
  doc["protocol"] = protocol;
  doc["protocolName"] = protocolName;
  doc["code"] = code;
  doc["size"] = size;
  doc["value"] = value;
  //  doc["code"] = String(code);

  String resp;
  serializeJson(doc, resp);
  server.send(200, "text/jsob", resp);
  setGreen();
  //  irsend.send(protocol, value, size);

}

void heartBeat() {
  display.clear();
  StaticJsonDocument<200> doc;
  doc["ip"] =  WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["ssid"] = SSID;
  doc["nerveHost"] = NERVE_SERVER_HOST;
  doc["nervePort"] = NERVE_SERVER_PORT;
  doc["version"] = VERSION;
  doc["os"] = HARDWARE;
  doc["isConnectedToNerve"] = isConnectedToNerve;
  isConnectedToNerve = true;
  String resp;
  serializeJson(doc, resp);
  server.send(200, "text/json", resp);
  timeSinceLastNerveCheck = millis();
}


void notifyNerve() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "connecting to nerve");
  display.drawString(0, 40,String(NERVE_SERVER_HOST) + ":" + String(NERVE_SERVER_PORT) );
  display.display();
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(nerveURL.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
//      display.clear();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
      isConnectedToNerve = true;
      server.send(200, "text/plain", "conneted to nerve");
//      display.drawString(0, 20, "connected to nerve");
      timeSinceLastNerveCheck = millis();
      display.display();
//      delay(2000);
    }
    else {
     
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      isConnectedToNerve = false;
      server.send(404, "text/plain", "unable to connet to nerve");
//      display.drawString(0, 20, "unable to connet to nerve");
    }
    display.display();
  }else{
     server.send(404, "text/plain", "unable to connet to nerve");
//     connectToWifi();
  }
  if (!isConnectedToNerve){
    delay(10000);
    notifyNerve();
  }
  

}
void handleNotFound() {

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

}



void getIRSensorData(decode_type_t *protocol, String *protocolName, String *value, uint64_t *code, uint16_t *size ) {

  //  Serial.print("we are in getIRSensorData \n" );
  while (irrecv.decode(&results)) {
    Serial.print("\nwe are in getIRSensorData \n" );
    *code = results.value;
    String temp = uint64ToString(results.value, HEX);
    *value = temp;
    *size =  results.bits;
    decode_type_t protocolTemp = results.decode_type;
    *protocol = protocolTemp;
    *protocolName = typeToString(protocolTemp).c_str();
    serialPrintUint64(results.value, HEX);
    //    irsend.send(protocol, results.value, size);
    String tempHex = resultToHexidecimal(&results);
    Serial.print("\nresultToHexidecimal " +  tempHex + "\n") ;
    //    resultToHexidecimal(results)

    irrecv.resume();
  }

}

void setRed () {
  //  (255,0,0)
  analogWrite(Led_Red, 255);
  analogWrite(Led_Green, 0);
  analogWrite(Led_Blue, 0);
}

void setGreen() {
  //  (0,128,0) forest green
  analogWrite(Led_Red, 0);
  analogWrite(Led_Green, 255);
  analogWrite(Led_Blue, 0);
}

void setBlue() {
  //  (0,0,255) blue
  analogWrite(Led_Red, 0);
  analogWrite(Led_Green, 0);
  analogWrite(Led_Blue, 255);
}
