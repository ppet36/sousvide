/**
 * SousVide controller for ESP12F or ESP07.
 *
 * Pins asignment:
 *
 * ADC    - water level sensor.
 * GPIO0  - triac switch for heater.
 * GPIO2  - triac switch for pump.
 * GPIO4  - OneWire DS18B20
 * GPIO5  - green status led.
 *
 * Code uses ArduinoJson, SPIFFS and OTA library.
 *
 * @author Pavel Petrzela
*/
#include <AutoPID.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

// WIFI
#define WIFI_AP_SSID "SousVideCooker"
#define WIFI_AP_PWD "serepesnanocnik"
#define WEB_SERVER_PORT 80

#define PIN_WATER_LEVEL A0
#define PIN_HEATER      12
#define PIN_PUMP        13
#define PIN_ONEWIRE     4
#define PIN_STATUS      5

// PWM heater window size
#define PWM_HEATER_WINDOW_SIZE    5000

// PID settings
#define DEFAULT_KP 0.12
#define DEFAULT_KI 0.0003
#define DEFAULT_KD 0
#define DEFAULT_PID_BANG 1.0

// DS18B20 temp is readed once 800ms
#define TEMP_READ_DELAY 800

// ADC value (greater values means enough water)
#define WATER_LEVEL_LIMIT  350

// States
#define ST_OFF      0x00
#define ST_NO_WATER 0x01
#define ST_RUNNING  0x02
#define ST_FINISHED 0x03

// Current eeprom magic for detecting unconfigured module
#define MAGIC 0xAA

// EEPROM structure for configuration
struct SvConfiguration {
  int magic;
  double kp;
  double ki;
  double kd;
  double bang;
};

// DS18B20
OneWire oneWire (PIN_ONEWIRE);
DallasTemperature temperatureSensors (&oneWire);

// PID values
double curTemperature, reqTemperature;
bool heaterVal;

// PID
AutoPIDRelay myPID (&curTemperature, &reqTemperature, &heaterVal, PWM_HEATER_WINDOW_SIZE, DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);

// Configuration server
ESP8266WebServer server (WEB_SERVER_PORT);

// Water level indication
int waterLevelAdc;
bool waterLevel;

// Timer; requested time (mins)
unsigned long timerStopTime = 0UL;
int timerMins = 0;
bool timerTargetTempReached = true;

// Configuration
SvConfiguration config;

// Current state
int state;

// Last heater on time
unsigned long lastHeaterOnTime = 0;
unsigned long lastHeaterOffTime = millis();


/**
 * setup() functions.
*/
void setup() {
  Serial.begin (115200);

  waterLevel = false;

  pinMode (PIN_HEATER, OUTPUT);
  pinMode (PIN_PUMP, OUTPUT);
  pinMode (PIN_STATUS, OUTPUT);

  digitalWrite (PIN_HEATER, LOW);
  digitalWrite (PIN_PUMP, LOW);
  digitalWrite (PIN_STATUS, LOW);

  delay (5000);

  // network FW upgrade
  ArduinoOTA.setHostname ("SousVide");
  ArduinoOTA.begin();

  // filesystem
  SPIFFS.begin();

  // init DS18B20
  temperatureSensors.begin();
  temperatureSensors.requestTemperatures();
  while (!updateTemperature()) {}

  reqTemperature = 0.0;

  // read configuration
  EEPROM.begin (sizeof (SvConfiguration));
  EEPROM.get (0, config);

  if (config.magic != MAGIC) {
    // if magic does not match, update with default values
    memset (&config, 0, sizeof (SvConfiguration));
    config.magic = MAGIC;
    config.kp = DEFAULT_KP;
    config.ki = DEFAULT_KI;
    config.kd = DEFAULT_KD;
    config.bang = DEFAULT_PID_BANG;
    EEPROM.put (0, config);
  }

  EEPROM.end();

  // init PID
  updatePidByConfig();

  WiFi.disconnect(true);

  WiFi.mode(WIFI_AP);

  // start WiFi
  WiFi.hostname (F("SousVide"));

  if (WiFi.softAP (WIFI_AP_SSID, WIFI_AP_PWD)) {
    Serial.print (F("Created AP: ")); Serial.println (WIFI_AP_SSID);
  }
  
  delay (500);

  server.on ("/cmd", HTTP_POST, wsHandleCmd);
  server.on ("/setup", wsHandleSetup);
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "404: Not Found");
    }
  });

  server.begin();
  Serial.print (F("Created HTTP server at ")); Serial.print (WiFi.softAPIP()); Serial.print (F(":")); Serial.println (WEB_SERVER_PORT);

  state = ST_OFF;

  delay(500);
}

/**
 * Update PID by configuration.
*/
void updatePidByConfig() {
  myPID.setBangBang (config.bang);
  myPID.setGains (config.kp, config.ki, config.kd);
}

/**
 * Simple routine to get content type of file.
 *
 * @param filename filename.
 * @return String content type.
*/
String getContentType (String filename) {
  if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith (".png")) {
    return "image/png";
  } else {
    return "text/plain";
  }
}

/**
 * Handles SPIFFS file read.
 *
 * @param path path.
 * @return bool success?
*/
bool handleFileRead (String path) {
  Serial.print(F("handleFileRead (")); Serial.print (path); Serial.print (F(")"));

  if (path.endsWith("/")) path += "index.html";
  String pathWithGz = path + ".gz";

  String foundPath;

  if (SPIFFS.exists (path)) {
    foundPath = path;
  } else if (SPIFFS.exists (pathWithGz)) {
    foundPath = pathWithGz;
  } else {
    Serial.println (F(" File Not Found"));
    return false;
  }

  String contentType = getContentType (path);

  Serial.print (F(" -> ")); Serial.print (foundPath);

  File file = SPIFFS.open (foundPath, "r");
  if (foundPath.endsWith (".gz")) {
    server.sendHeader (F("Content-Encoding"), F("gzip"));
  }
  server.sendHeader (F("Cache-Control"), F("public,max-age=86400"));

  char buf[1024];
  int siz = file.size();

  int toTransfer = siz;

  server.setContentLength (siz);
  server.send (200, contentType, "");
  delay (250);

  while (siz > 0) {
    size_t len = std::min ((int)(sizeof(buf) - 1), siz);
    file.read ((uint8_t *)buf, len);
    server.client().write ((const char*)buf, len);
    Serial.print (F("."));
    delay (100);
    yield();
    siz -= len;
  }
  file.close();

  server.client().stop();

  Serial.print(F(" sent ")); Serial.print (toTransfer); Serial.println (F(" bytes; SUCCESS"));
  return true;
}

/**
 * Update DS18B20 temperature.
 *
 * @return bool success?
*/
bool updateTemperature() {
  static unsigned long lastTempUpdate = millis();

  if ((millis() - lastTempUpdate) > TEMP_READ_DELAY) {
    curTemperature = temperatureSensors.getTempCByIndex (0);

    if ((curTemperature >= reqTemperature) && !timerTargetTempReached) {
      timerStopTime = millis() + (timerMins * 60000UL);
      timerTargetTempReached = true;
    }

    lastTempUpdate = millis();
    temperatureSensors.requestTemperatures();
    return true;
  } else {
    return false;
  }
}

/**
 * Maintains heater.
*/
void maintainHeater() {
  myPID.run();

  if (waterLevel) {
    digitalWrite (PIN_HEATER, heaterVal ? HIGH : LOW);

    static bool lastHeaterStateChange = heaterVal;
    if (heaterVal != lastHeaterStateChange) {
      if (heaterVal) {
        lastHeaterOffTime = millis() - lastHeaterStateChange;
      } else {
        lastHeaterOnTime = millis() - lastHeaterStateChange;
      }

      lastHeaterStateChange = millis();
    }
  } else {
    digitalWrite (PIN_HEATER, LOW);
  }
}

/**
 * Updates status led.
*/
void checkWaterLevel() {
  waterLevelAdc = analogRead (PIN_WATER_LEVEL);

  waterLevel = (waterLevelAdc > WATER_LEVEL_LIMIT);

  if (!waterLevel) {
    state = ST_NO_WATER;
    digitalWrite (PIN_HEATER, LOW);
    digitalWrite (PIN_PUMP, LOW);
  } else {
    if (reqTemperature > 0.0) {
      digitalWrite (PIN_PUMP, HIGH);
    } else {
      digitalWrite (PIN_PUMP, LOW);
    }

    if (state == ST_NO_WATER) {
      state = ST_OFF;
    }
  }
}

/**
 * Updates status led.
*/
void updateStatusLed() {
  static unsigned long ledLastBlink = millis();

  bool curLedState = (digitalRead(PIN_STATUS) == HIGH);

  int onTime, offTime;

  if (state == ST_OFF) {
    onTime = 250;
    offTime = 3000;
  } else if (state == ST_NO_WATER) {
    onTime = 0;
    offTime = 1;
  } else if (state == ST_RUNNING) {
    onTime = 300;
    offTime = 300;
  } else if (state == ST_FINISHED) {
    onTime = 1;
    offTime = 0;
  }

  if (onTime == 0) {
    digitalWrite (PIN_STATUS, LOW);
  } else if (offTime == 0) {
    digitalWrite (PIN_STATUS, HIGH);
  } else {
    int time = (curLedState ? onTime : offTime);

    if ((millis() - ledLastBlink) >= time) {
      if (curLedState) {
        digitalWrite (PIN_STATUS, LOW);
      } else {
        digitalWrite (PIN_STATUS, HIGH);
      }
      ledLastBlink = millis();
    }
  }
}

/**
 * Command handler.
*/
void wsHandleCmd() {
  StaticJsonDocument<200> newDoc;
  DeserializationError err = deserializeJson (newDoc, server.arg ("plain"));
  if (err) {
    server.send (400, "text/plain", "Bad request!");
    return;
  }
  JsonObject& newjson = newDoc.as<JsonObject>();

  Serial.print (F("CMD=")); Serial.println (server.arg ("plain"));

  JsonVariant v = newjson["reqTemperature"];
  if (v.success()) {
    reqTemperature = v | 0.0;
    timerStopTime = 0UL;

    if (reqTemperature > 0.0) {
      state = ST_RUNNING;
    } else if ((state == ST_RUNNING) || (state == ST_FINISHED)) {
      state = ST_OFF;
    }

    Serial.print (F("Setting requested temperature to ")); Serial.println (reqTemperature);
  }

  v = newjson["timerMins"];
  if (v.success()) {
    timerMins = v | 0;

    if (timerMins == 0) {
      timerStopTime = 0UL;
      timerTargetTempReached = true;
      Serial.println (F("Reset timer..."));
    } else {
      timerStopTime = millis() + (timerMins * 60000UL);
      timerTargetTempReached = false;
      Serial.print (F("Setting timer to ")); Serial.print (v | 0); Serial.println (F(" mins."));
    }
  }

  int heaterPercent;
  if (state == ST_RUNNING) {
    if ((lastHeaterOnTime == 0) || (lastHeaterOffTime == 0)) {
      heaterPercent = digitalRead(PIN_HEATER) ? 100 : 0;
    } else {
      heaterPercent = lastHeaterOffTime * 100L / (lastHeaterOnTime + lastHeaterOffTime);
    }
  } else {
    heaterPercent = 0;
  }

  String resp = F("{ ");
  resp += F("\"curTemperature\" : ");
  resp += String(curTemperature, 1);
  resp += F(", \"reqTemperature\" : ");
  resp += String(reqTemperature, 1);
  resp += F(", \"waterLevel\" : ");
  resp += String(waterLevel);
  resp += F(", \"heaterState\" : ");
  resp += String(digitalRead (PIN_HEATER));
  resp += F(", \"heaterPercent\" : ");
  resp += String(heaterPercent);
  resp += F(", \"pumpState\" : ");
  resp += String(digitalRead (PIN_PUMP));

  if (timerMins > 0) {
    resp += F(", \"remainingMins\" : ");
    if (timerTargetTempReached) {
      resp += String((timerStopTime - millis()) / 60000UL);
    } else {
      resp += String(timerMins);
    }
  }

  resp += F(", \"timerStateString\" : \"");
  if (timerMins > 0) {
    if (timerTargetTempReached) {
      unsigned long millisTimerTime = timerStopTime - millis();
      if (millisTimerTime < 60000UL) {
        resp += String(millisTimerTime / 1000UL);
        resp += F(" secs remaining");
      } else {
        resp += String(millisTimerTime / 60000UL);
        resp += F(" mins remaining");
      }
    } else {
      resp += F("Reaching target temperature...");
    }
  } else {
    resp += F("Set timer");
  }
  resp += F("\"");

  resp += F(", \"state\" : \"");
  switch (state) {
    case ST_OFF :
      resp += "OFF";
    break;
    case ST_NO_WATER :
      resp += "!NO_WATER!";
    break;
    case ST_RUNNING : 
      resp += "RUNNING";
    break;
    case ST_FINISHED :
      resp += "FINISHED";
    break;
    default : break;
  }

  resp += F("\" }");

  Serial.print (F("RESPONSE= "));
  Serial.println (resp);

  server.send (200, "text/json", resp);
}

/**
 * Convert variant to double.
 *
 * @param v variant.
 * @return double double.
*/
double variantToDouble (JsonVariant& v) {
  String s = v | "";
  return s.toFloat();
}

/**
 * Handles configuration.
*/
void wsHandleSetup() {
  if (server.method() == HTTP_POST) {
    StaticJsonDocument<200> newDoc;
    DeserializationError err = deserializeJson (newDoc, server.arg ("plain"));
    if (err) {
      server.send (400, "text/plain", "Bad request!");
      return;
    }
    JsonObject& newjson = newDoc.as<JsonObject>();

    Serial.print (F("CMD=")); Serial.println (server.arg ("plain"));

    JsonVariant v = newjson["kp"];
    if (v.success()) {
      config.kp = variantToDouble (v);
    }
    v = newjson["ki"];
    if (v.success()) {
      config.ki = variantToDouble (v);
    }
    v = newjson["kd"];
    if (v.success()) {
      config.kd = variantToDouble (v);
    }
    v = newjson["bang"];
    if (v.success()) {
      config.bang = variantToDouble (v);
    }

    // update PWM parameters
    updatePidByConfig();

    // save to EEPROM
    EEPROM.begin (sizeof (SvConfiguration));
    EEPROM.put (0, config);
    EEPROM.end();
  } else {
    server.sendHeader (F("Cache-Control"), F("private,no-cache,must-revalidate"));
  }

  // response GET/POST is always JSON configuration structure
  String resp = F("{ ");
  resp += F("\"kp\" : ");
  resp += String(config.kp, 3);
  resp += F(", \"ki\" : ");
  resp += String(config.ki, 3);
  resp += F(", \"kd\" : ");
  resp += String(config.kd, 3);
  resp += F(", \"bang\" : ");
  resp += String(config.bang, 1);
  resp += F(" }");

  server.send (200, "text/json", resp);
}

/**
 * Updates timer function.
*/
void updateTimer() {
  if (timerMins > 0) {
    if (timerTargetTempReached && (millis() > timerStopTime)) {
      reqTemperature = 0.0;
      timerStopTime = 0UL;
      timerMins = 0;
      state = ST_FINISHED;
      Serial.println (F("Timer end triggered..."));
    }
  }
}

/**
 * Loop function.
*/
void loop() {
  ArduinoOTA.handle();

  updateTimer();

  updateTemperature();

  checkWaterLevel();

  updateStatusLed();

  maintainHeater();

  server.handleClient();

  static unsigned long lastStateLog = millis();
  if (millis() - lastStateLog > 5000UL) {
    Serial.print (F("STATE: "));
    Serial.print (curTemperature);
    Serial.print (F(" -> "));
    Serial.print (reqTemperature);
    Serial.print (F(", HEAT: "));
    Serial.print (digitalRead (PIN_HEATER));
    Serial.print (F(", WATER: "));
    Serial.print (waterLevel);
    Serial.print (F(" ("));
    Serial.print (waterLevelAdc);
    Serial.print (F("), PUMP: "));
    Serial.print (digitalRead (PIN_PUMP));
    Serial.print (F(", WIFISTAT: "));
    Serial.print (WiFi.softAPgetStationNum());
    Serial.println();

    lastStateLog = millis();
  }

  delay(250);
}

