#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>

#include <Arduino.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>

#include <FS.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif

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
bool shouldSaveConfig = true;

//display
#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
#include "SH1106Wire.h"
#include "images.h"
#include "config.h"
typedef void (*Display)(void);
// Add this library: https://github.com/markszabo/IRremoteESP8266
#include <IRremoteESP8266.h>
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%



//const uint16_t kRecvPin = 14;
#define Relay_00 2        //mesmo que D4 not in use we are using from 1
#define Relay_01 12 // D6
#define Relay_02 13  // D7
#define Relay_03 14  // D5
#define Relay_04 15  // D8
#define Relay_05 16  // D0

#define Relay_01 14 // D5
#define Relay_02 12  // D6
#define Relay_03 13  // D7
#define Relay_04 15  // D8
#define Relay_05 16  // D0


#define NUM_RELAYS  3
//int relayGPIOs[NUM_RELAYS] = {14, 12, 13, 15, 16};
int relayGPIOs[NUM_RELAYS] = {14, 12, 13};


//#define Relay_07 16  //
//#define Relay_08 16  // D

// GPIO to use to control the IR LED circuit. Recommended: 4 (D2).
//const uint16_t kIrLedPin = 12;

// The Serial connection baud rate.
// NOTE: Make sure you set your Serial Monitor to the same speed.
const uint32_t kBaudRate = 115200;

//const int Led_Red = 2;
//const int Led_Green = 13;
//const int Led_Blue = 15;

// As this program is a special purpose capture/resender, let's use a larger
// than expected buffer so we can handle very large IR messages.
const uint16_t kCaptureBufferSize = 1024;  // 1024 == ~511 bits

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
const uint8_t kTimeout = 50;  // Milli-Seconds

// kFrequency is the modulation frequency all UNKNOWN messages will be sent at.
const uint16_t kFrequency = 38000;  // in Hz. e.g. 38kHz.


//D1 -SCL 5
//D2 - SDA 4


SH1106Wire display(0x3c, SDA, SCL); // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h
//SSD1306Wire display(0x3c, 4, 5); // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h
// The IR transmitter.
//IRsend irsend(kIrLedPin);
// The IR receiver.
//IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, false);
// Somewhere to store the captured message.
decode_results results;

//const String ssid = "izingawireless_2.4G";
char* password = SSID_PASSWPRD;
String nerveURL =  "Not Set";
String nerveHostNPort = "Not Set";
char* nerveServerHost = "";
char* nerveServerPort = "";
char* licenseKey = "";
bool isConnectedToNerve = false;
String firmVersion = "1.0.1";

ESP8266WebServer server(PORT);

int displayMode = 0;
int counter = 1;


long timeSinceLastModeSwitch = 0;
long timeSinceLastWifiChecked = 0;
long timeSinceLastNerveCheck = 0;


WiFiClient wifiClient;
void setup(void) {

  Serial.begin(115200);
  Serial.println("We are starting here ");

  //    SPIFFS.format();
  // Set all relays to off when the program starts - if set to Normally Open (NO), the relay is off when you set the relay to HIGH
  for (int i = 1; i <= NUM_RELAYS; i++) {
    pinMode(relayGPIOs[i - 1], OUTPUT);
    digitalWrite(relayGPIOs[i - 1], HIGH);
  }

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/nerve_config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/nerve_config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        if (size > 0) {
          Serial.println("file Data size - " + String(size));

          //#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
          DynamicJsonDocument json(1024);
          auto deserializeError = deserializeJson(json, buf.get());
          serializeJson(json, Serial);
          if ( ! deserializeError ) {
            //#else
            //        DynamicJsonBuffer jsonBuffer;
            //        JsonObject& json = jsonBuffer.parseObject(buf.get());
            //        json.printTo(Serial);
            //        if (json.success()) {
            //#endif

            // extract the data
            JsonObject object = json.as<JsonObject>();
            Serial.println("\nparsed json");
            const char* host1 = object["nerve_server_host"];
            const char* port1 = object["nerve_server_port"];
            const char* license1 = object["license_key"];
            //          strcpy(nerveServerHost, object["nerve_server_host"]);
            //          strcpy(nerveServerPort, object["nerve_server_port"]);
            //          strcpy(licenseKey, object["license_key"]);
            nerveURL =  "https://" + String(host1) + ":" + String(port1) + "/neuron/v3/node?licensekey=" + String(license1) + "&machineID=" + WiFi.macAddress() + "&name=" + WiFi.macAddress() + "&relay=" + String(digitalRead(Relay_01));
            nerveHostNPort = String(host1) + ":" + String(port1);
            Serial.println("file nerveURL " + nerveURL);
          }
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }

      else {
        Serial.println("unable to read file");
      }
    } else {
      Serial.println("file not found");

      File configFile = SPIFFS.open("/nerve_config.json", "w");
      if (!configFile) {
        Serial.println("file create failed");
      }
    }

  }
  Serial.println("Got value from file, now going to set wifi module");
  //  pinMode(Led_Red, OUTPUT);
  //  pinMode(Led_Green, OUTPUT);
  //  pinMode(Led_Blue, OUTPUT);
  //  pinMode(TRIGGER_PIN, INPUT);
  //  irrecv.enableIRIn();  // Start up the IR receiver.
  //  irsend.begin();       // Start up the IR sender.
  // Initialising the UI will init the display too.
  display.init();

  Serial.println("initiating dispaly");

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  //  digitalWrite(BUILTIN_LED, HIGH);
  setupWifi();
  //  WiFi.begin(SSID, password);
  Serial.println("Wifi setup done");

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
  //  nerveURL =  "https://" + String(nerveServerHost) + ":" + String(nerveServerPort) + "/neuron/v3/node?licensekey=" + String(licenseKey) + "&machineID=" + WiFi.macAddress() + "&name=" + WiFi.macAddress() + "&relay=" + String(digitalRead(Relay_01));
  // over the air started
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setPassword("!z!ng@");
  ArduinoOTA.onStart([]() {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "OTA Started");
    display.display();
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "OTA Completed");
    display.display();
    Serial.println("Start");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "OTA Error");
    display.display();
    Serial.println("Start");
    
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
      display.drawString(0, 14, "Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
      display.drawString(0, 14, "Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
      display.drawString(0, 14, "Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
      display.drawString(0, 14, "Receive Failed");
    }
    else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
      display.drawString(0, 14, "End Failed");
    }
  });
  ArduinoOTA.begin();

  server.on("/", heartBeat);
  server.on("/test", handleTest);
  server.on("/notifynerve", notifyNerve);
  server.on("/run", handleRun);
  server.on("/info", heartBeat);
  server.on("/v2/heartbeat", heartBeat);
  server.on("/record", handleRecord);
  server.on("/reset", resetConfig);
  server.on("/relay/on", relayOn);
  server.on("/relay/off", relayOff);
  server.on("/relay/status", relayStatus);

  server.onNotFound(handleNotFound);


  server.begin();
  //  setGreen();
  Serial.println("HTTP Server Started");
  notifyNerve();
}

void saveConfigCallback () {
  Serial.println("Should save config now");
  shouldSaveConfig = true;
}

// setup wifi
void setupWifi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  //  delay(3000);
  Serial.println("\n Starting setting wifi");

  wm.setSaveConfigCallback(saveConfigCallback);

  //  pinMode(TRIGGER_PIN, INPUT);


  if (wm_nonblocking) wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(1200);
  new  (&nerve_server_host)WiFiManagerParameter("nerveServerHost", "nerve server ip", "", 40);
  wm.addParameter(&nerve_server_host);
  new  (&nerve_server_port)WiFiManagerParameter ("nerveServerPort", "nerve server port", "", 6);
  wm.addParameter(&nerve_server_port);
  new (&license_key)WiFiManagerParameter ("licenseKey", "License Key", "", 40);
  wm.addParameter(&license_key);
  wm.setBreakAfterConfig(true);
  wm.setSaveParamsCallback(saveParamCallback);
  bool res;
  displayWifiConfig();
  res = wm.autoConnect(AP_NAME, AP_PASSWORD);
  if (!res) {
    Serial.println("Failed to connect or hit timeout");
    ESP.restart();
  }
  else {


    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }


}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback() {
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("nerveServerHost"));
  nerveURL =  "https://" + getParam("nerveServerHost") + ":" + getParam("nerveServerPort") + "/neuron/v3/node?licensekey=" + getParam("licenseKey") + "&machineID=" + WiFi.macAddress() + "&name=" + WiFi.macAddress() + "&relay=" + String(digitalRead(Relay_01));
  nerveHostNPort = getParam("nerveServerHost") + ":" + getParam("nerveServerPort") ;
  Serial.println("nerveURL " + nerveURL);
  //  strcpy(nerveServerHost, nerve_server_host.getValue());
  //  strcpy(nerveServerPort, nerve_server_port.getValue());
  //  strcpy(licenseKey, license_key.getValue());
  Serial.println("The values in the file are: ");


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["nerve_server_host"] = getParam("nerveServerHost");
    json["nerve_server_port"] =  getParam("nerveServerPort");
    json["license_key"] =  getParam("licenseKey");


    File configFile = SPIFFS.open("/nerve_config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6

    Serial.println("The values in the file are 6:  ");
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    Serial.println("The values in the file are: ");
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }

  Serial.println("nerveURL " + nerveURL);

  timeSinceLastWifiChecked = millis();
  //notifyNerve();
}

int getRelayNumber() {
  int relayNumber = 0;
  if (server.arg("switch") != "") {
    relayNumber = server.arg("switch").toInt();
  }
  return relayNumber;
}

void relayOn() {
  int relayNumber = getRelayNumber();
  if (relayNumber > 6 || relayNumber < 1) {
    server.send(400, "text/json", "invalid switch number");
    return;
  }


  digitalWrite(relayGPIOs[relayNumber - 1], HIGH);

  StaticJsonDocument<200> doc;
  doc["switch"] = relayNumber;
  doc["relay"] = relayGPIOs[relayNumber - 1];
  doc["status"] = String(digitalRead(relayGPIOs[relayNumber - 1]));

  String resp;
  serializeJson(doc, resp);
  server.send(200, "text/jsob", resp);
}

void relayOff() {
  int relayNumber = getRelayNumber();
  if (relayNumber > 6 || relayNumber < 1) {
    server.send(400, "text/json", "invalid switch number");
    return;
  }

  digitalWrite(relayGPIOs[relayNumber - 1], LOW);
  StaticJsonDocument<200> doc;
  doc["switch"] = relayNumber;
  doc["relay"] = relayGPIOs[relayNumber - 1];
  doc["status"] = String(digitalRead(relayGPIOs[relayNumber - 1]));

  String resp;
  serializeJson(doc, resp);
  server.send(200, "text/jsob", resp);
}

void relayStatus() {
  StaticJsonDocument<200> doc;
  //  JsonObject &root = jsonBuffer.createObject();x
  for (int i = 1; i <= NUM_RELAYS; i++) {
    //    pinMode(relayGPIOs[i - 1], OUTPUT);
    doc["switch_" + String(i)] = String(digitalRead(relayGPIOs[i - 1]));

  }

  String resp;
  serializeJson(doc, resp);
  server.send(200, "text/jsob", resp);

}

void resetConfig() {
  server.send(200, "text/json", "reset");
  wm.resetSettings();
  wm.resetSettings();
  SPIFFS.format();
  ESP.reset();
}


void connectToWifi() {
  int count = 0;
  counter = 20;
  while (WiFi.status() != WL_CONNECTED) {
    drawProgressBarForWifiConnection();
    //    for (int val = 255; val > 0; val--) {
    //      analogWrite (Led_Red, val);
    //      analogWrite (Led_Blue, 255 - val);
    //      analogWrite (Led_Green, 128 - val);
    //      delay (5);
    //      val = val - 5;
    //    }

    //    for (int val = 0; val < 255; val++) {
    //      analogWrite (Led_Red, val);
    //      analogWrite (Led_Blue, 255 - val);
    //      analogWrite (Led_Green, 128 - val);
    //      val = val + 5;
    //      delay (5);
    //    }
    if (count > 10 && count % 2 == 0) {
      displayWifiDetails();
      //      delay(WIFI_DISPLAY_DURATION);
    }

    count ++ ;
    Serial.print(".");
  }
  timeSinceLastWifiChecked = millis();
  notifyNerve();
}


Display displayInfoMode[] = { displayInfo, drawIcon, drawChargingAnimation};
int displayModeLength = (sizeof(displayInfoMode) / sizeof(Display));

void loop(void) {

  //  wm.process();
  display.clear();
  ArduinoOTA.handle();
  if ( digitalRead(TRIGGER_PIN) == LOW) {

    //    //reset settings - for testing
    //        wm.resetSettings();
    //        SPIFFS.format();
    //        ESP.reset();
    //

    //      setupWifi();
  }
  displayInfoMode[displayMode]();
  //  digitalWrite(BUILTIN_LED, HIGH);
  //  setGreen();
  Serial.println("We are in loop, handling server/client connection ");
  server.handleClient();
  Serial.println("We are in loop, handling server/client connection done ");
  //  setGreen();
  //  digitalWrite(BUILTIN_LED, LOW);
  if (millis() - timeSinceLastModeSwitch > INFO_DISPLAY_DURATION) {
    displayMode = (displayMode + 1)  % displayModeLength;
    timeSinceLastModeSwitch = millis();
  }
  if (millis() - timeSinceLastModeSwitch > WIFI_CHECK_DURATION) {
    //    connectToWifi();
  }
  if (millis() - timeSinceLastNerveCheck > NERVE_CHECK_DURATION) {

    notifyNerve();
    timeSinceLastNerveCheck = millis();
  }
  display.display();


}

void drawIcon() {
  display.clear();
  display.drawXbm(0, 15, robusTestIconWidth, robusTestIconHeight, robusTestIconBits);
}

void drawChargingAnimation() {

  uint8_t width;

  display.clear();

  display.drawXbm(0, 0, 128, 64, battery_bitmap);
  display.display();
  String chargingState = "";
  for (int i = 1; i <= NUM_RELAYS; i++) {
    //    pinMode(relayGPIOs[i - 1], OUTPUT);
    if (digitalRead(relayGPIOs[i - 1]) == HIGH) {
      chargingState = chargingState + ", " + String(i);
    }

  }
  if (1 == 1) {
    for (int i = 0; i < 65; i++) {
      //  digitalRead(Relay_01)
      width = CHARGE_AREA_START_X + i;
      display.drawRect(CHARGE_AREA_START_X, CHARGE_AREA_START_Y, (width), CHARGE_AREA_HEIGHT);

      display.fillRect(CHARGE_AREA_START_X, CHARGE_AREA_START_Y, (width), CHARGE_AREA_HEIGHT);
      //   display.drawBox(CHARGE_AREA_START_X, CHARGE_AREA_START_Y, width, CHARGE_AREA_HEIGHT);
      delay(30);
      display.drawString(10, 50, "Charging - " + chargingState);
      display.display();

    }
  } else {
    display.drawString(30, 50, "Not Charging");
    display.display();
  }


}


void displayWifiDetails() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "MAC - " + WiFi.macAddress());
  display.drawString(0, 14, wm.getWiFiSSID() );
  display.drawString(0, 28, wm.getWiFiPass());
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
  display.drawString(0, 14, wm.getWiFiSSID() );
  display.drawString(0, 28, "IP - " +  WiFi.localIP().toString());
  display.drawString(0, 42, nerveHostNPort);
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
  //  irsend.sendNEC(0xA25DFA05, 32);

  delay(1000);
  //  irsend.sendSAMSUNG(3772802968, 32);
  //  irsend.sendSAMSUNG(0xE0E06798, 32);
  server.send(200, "text/plain", "Sat OK");

}


void handleRun() {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 20, "Playing");
  display.display();
  //  setBlue();
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
  bool success = false;

  //  bool success = irsend.send(protocol, code, size);


  if (!success) {
    server.send(400, "text/json", "unable to run remote command");
  } else {
    server.send(200, "text/json", "sent remote command");
  }

  //  setGreen();
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

  decode_type_t protocol;
  String protocolName;
  String value ;
  uint16_t size ;
  uint64_t code;
  size = 0;
  while (size < 1) {
    //    getIRSensorData(&protocol, &protocolName, &value, &code, &size);
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
  //  setGreen();
  //  irsend.send(protocol, value, size);

}

void heartBeat() {
  display.clear();
  StaticJsonDocument<200> doc;
  doc["ip"] =  WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["ssid"] = SSID;
  doc["nerveHost"] = String(nerveServerHost) ;
  doc["nervePort"] = String(nerveServerPort) ;
  doc["version"] = VERSION;
  doc["os"] = HARDWARE;
  doc["relay"] = String(digitalRead(Relay_01));
  doc["isConnectedToNerve"] = isConnectedToNerve;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["firmVersion"] = firmVersion;
      isConnectedToNerve = true;
      String resp;
      serializeJson(doc, resp);
      server.send(200, "text/json", resp);
      timeSinceLastNerveCheck = millis();
}


void notifyNerve() {

  //  nerveURL =  "https://" + String(nerveServerHost) + ":" + String(nerveServerPort) + "/neuron/v3/node?licensekey=" + String(licenseKey) + "&machineID=" + WiFi.macAddress() + "&name=" + WiFi.macAddress() + "&relay=" + String(digitalRead(Relay_01));

  Serial.println("connecting to nerve url  - "  + nerveURL );
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "connecting to nerve");
  display.drawString(0, 40, String(nerveServerHost)  + ":" + String(nerveServerPort)  );
  display.display();
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(wifiClient, nerveURL.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      //      display.clear();
      Serial.println("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
      isConnectedToNerve = true;
      Serial.println("Connected to nerve");
      server.send(200, "text/plain", "conneted to nerve");
      //      display.drawString(0, 20, "connected to nerve");
      timeSinceLastNerveCheck = millis();
      display.display();
      //      delay(2000);
    }
    else {

      Serial.println("Error code: ");
      Serial.println(httpResponseCode);
      isConnectedToNerve = false;
      server.send(404, "text/plain", "unable to connet to nerve");
      //      display.drawString(0, 20, "unable to connet to nerve");
    }
    display.display();
    //    wifiClient.abort();
    http.end();
  } else {

    server.send(404, "text/plain", "unable to connet to nerve");
    //     connectToWifi();
  }

  //  if (!isConnectedToNerve) {
  //
  //    notifyNerve();
  //    delay(10000);
  //  }


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



//void getIRSensorData(decode_type_t *protocol, String *protocolName, String *value, uint64_t *code, uint16_t *size ) {
//
//  //  Serial.print("we are in getIRSensorData \n" );
//  while (irrecv.decode(&results)) {
//    Serial.print("\nwe are in getIRSensorData \n" );
//    *code = results.value;
//    String temp = uint64ToString(results.value, HEX);
//    *value = temp;
//    *size =  results.bits;
//    decode_type_t protocolTemp = results.decode_type;
//    *protocol = protocolTemp;
//    *protocolName = typeToString(protocolTemp).c_str();
//    serialPrintUint64(results.value, HEX);
//    //    irsend.send(protocol, results.value, size);
//    String tempHex = resultToHexidecimal(&results);
//    Serial.print("\nresultToHexidecimal " +  tempHex + "\n") ;
//    //    resultToHexidecimal(results)
//
//    irrecv.resume();
//  }
//
//}
//
//void setRed () {
//  //  (255,0,0)
//  analogWrite(Led_Red, 255);
//  analogWrite(Led_Green, 0);
//  analogWrite(Led_Blue, 0);
//}
//
//void setGreen() {
//  //  (0,128,0) forest green
//  analogWrite(Led_Red, 0);
//  analogWrite(Led_Green, 255);
//  analogWrite(Led_Blue, 0);
//}
//
//void setBlue() {
//  //  (0,0,255) blue
//  analogWrite(Led_Red, 0);
//  analogWrite(Led_Green, 0);
//  analogWrite(Led_Blue, 255);
//}
