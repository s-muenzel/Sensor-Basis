/* */

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include <PubSubClient.h>

#define DEBUGING
#define DEBUG_SERIAL
#ifdef DEBUG_SERIAL
#define D_BEGIN(speed)   Serial.begin(speed)
#define D_PRINT(...)     Serial.print(__VA_ARGS__)
#define D_PRINTLN(...)   Serial.println(__VA_ARGS__)
#define D_PRINTF(...)    Serial.printf(__VA_ARGS__)
#else
#define D_BEGIN(speed)
#define D_PRINT(...)
#define D_PRINTLN(...)
#define D_PRINTF(...)
#endif

#include <SimpleDHT.h>

#include "Zugangsinfo.h"

// Allgemeine Parameter
#ifdef DEBUGING
#define ZEIT_ZW_MESSUNGEN        5ll     /* wielange zwischen Sensor-Messungen(in Sekunden) */
#define ZEIT_ZW_NETZWERKFEHLER     5ll     /* bei WLAN oder MQTT Verbindungsproblemen (in Sekunden) */
#define ZEIT_ZW_NIEDRIGER_AKKU  10ll     /* wenn der Akku leer ist (in Sekunden) */
#else
#define ZEIT_ZW_MESSUNGEN        300ll     /* wielange zwischen Sensor-Messungen(in Sekunden) */
#define ZEIT_ZW_NETZWERKFEHLER     5ll     /* bei WLAN oder MQTT Verbindungsproblemen (in Sekunden) */
#define ZEIT_ZW_NIEDRIGER_AKKU  3600ll     /* wenn der Akku leer ist (in Sekunden) */
#endif

// Sensor DHT22
#define DHT22PIN 22
SimpleDHT22 dht22(DHT22PIN);

// Sensor Photowiderstand
#define RPHOTOPIN 34

// MQTT Muster Daten:
#define DEVICETYP	"Sensor"
#define DEVICEID	"WZTuF"
#define DEVICEORT1	"EG"
#define DEVICEORT2	"WZ"
#define DEVICEORT3	""
#define DEVICEART1	"T" // 1. Sensorwert: Temperatur
#define DEVICEART2	"F" // 2. Sensorwert: Rel. Feuchte
#define DEVICEART3	"H" // 3. Sensorwert: Helligkeit
#define DEVICEART4	"B" // 4. Sensorwert: Batteriestatus

#define MQTT_MUSTER_BASIS		DEVICETYP "/" DEVICEID "/" DEVICEORT1 "/" DEVICEORT2 "/" DEVICEORT3 "/"

#define MAX_NACHRICHT   40
#define MAX_THEMA       55 // MQTT Topic (siehe oben)

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[MAX_NACHRICHT];
char _thema[MAX_THEMA]; // topic

#define BOOT_NORMAL 0
#define BOOT_AKKU   1
#define BOOT_WLAN   2
#define BOOT_MQTT   3

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int bootNachricht = BOOT_NORMAL;

//////////////////////////////////////////////
// Deepsleep
#define uS_TO_S_FACTOR 1000000ll  /* Conversion factor for micro seconds to seconds */

/////////////////////////////////////////////////
// Normaler WiFi Part

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  D_PRINTLN();
  D_PRINT("Connecting to ");
  D_PRINTLN(ssid);

  WiFi.begin(ssid, password);

  int connect_trial_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    D_PRINT(".");
    ++connect_trial_count;
    if (connect_trial_count > 10) {
      D_PRINTLN("Fehler: kein WLAN, schlafe und versuche es spaeter wieder\n");
      bootNachricht = BOOT_WLAN;
      esp_sleep_enable_timer_wakeup(ZEIT_ZW_NETZWERKFEHLER * uS_TO_S_FACTOR);
      delay(50);
      esp_deep_sleep_start();
    }
  }

  D_PRINTLN("");
  D_PRINTLN("WiFi connected");
  D_PRINTLN("IP address: ");
  D_PRINTLN(WiFi.localIP());
}

///////////////////////////////////////////////////
// MQTT Part

void callback(char* topic, byte* payload, unsigned int length) {
  D_PRINTF("Message arrived [%s] ", topic);
  for (int i = 0; i < length; i++) {
    D_PRINT((char)payload[i]);
  }
  D_PRINTLN();
}

void reconnect() {
  // Loop until we're reconnected
  int connect_trial_count = 0;
  while (!client.connected()) {
#ifdef ESP8266
    String clientId = "E8266-";
    clientId += String((long unsigned int)(0xffff & ESP.getChipId()), HEX);
#endif
#ifdef ESP32
    String clientId = "E32__-";
    clientId += String((long unsigned int)(0xffff & ESP.getEfuseMac()), HEX);
    clientId += String((long unsigned int)(0xffff & (ESP.getEfuseMac() >> 32)), HEX);
#endif
    D_PRINTF("Verbinde mit MQTT Broker <%s> ", clientId.c_str());
    // Attempt to connect
    if (client.connect(clientId.c_str(), device_user, device_pw, MQTT_MUSTER_BASIS, 0, true, "tot", true)) {
      D_PRINTF("connected: <%s>\n", clientId.c_str());
    } else {
      D_PRINT("failed, rc=");
      D_PRINT(client.state());
      D_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
      ++connect_trial_count;
      if (connect_trial_count > 10) {
        D_PRINTLN("Fehler: kein MQTT, schlafe und versuche es spaeter wieder\n");
        bootNachricht = BOOT_MQTT;
        esp_sleep_enable_timer_wakeup(ZEIT_ZW_NETZWERKFEHLER * uS_TO_S_FACTOR);
        delay(50);
        esp_deep_sleep_start();
      }
    }
  }
}

void send_MSG(const char* d_art, int wert) {
  char _msg[MAX_NACHRICHT];
  snprintf (_msg, MAX_NACHRICHT, "%d", wert);
  send_MSG(d_art, _msg);
}

void send_MSG(const char* d_art, float wert) {
  char _msg[MAX_NACHRICHT];
  snprintf (_msg, MAX_NACHRICHT, "%f", wert);
  send_MSG(d_art, _msg);
}

void send_MSG(const char* d_art, const char* wert) {
  strncpy(msg, wert, MAX_NACHRICHT);
  snprintf (_thema, MAX_THEMA, "%s%s", MQTT_MUSTER_BASIS, d_art);
  D_PRINTF("Publish message: <%s>:<%s>", _thema, msg);
  if (client.publish(_thema, msg, true)) {
    D_PRINTLN(" good");
  } else {
    D_PRINTLN(" failed");
  }
}

void send_MSG_Other(const char* inhalt, bool retain) {
  snprintf (msg, MAX_NACHRICHT, "%s", inhalt);
  D_PRINTF("Publish message: <%s>:<%s>", MQTT_MUSTER_BASIS, msg);
  if (client.publish(MQTT_MUSTER_BASIS, msg, retain)) {
    D_PRINTLN(" good");
  } else {
    D_PRINTLN(" failed");
  }
}

//////////////////////////////////////////////////////
// Hauptprogramm

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
#ifdef DEBUGING
  // Zur Begrüßung 3 mal flash
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i < 2)
      delay(50);
  }
#endif

  D_BEGIN(115200);
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  D_PRINTF("Setup ESP32 Deepsleep fuer %lld Sekunden\n", ZEIT_ZW_MESSUNGEN);
  esp_sleep_enable_timer_wakeup(ZEIT_ZW_MESSUNGEN * uS_TO_S_FACTOR);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  switch(bootNachricht) {
    default:
    case BOOT_NORMAL:
      sprintf(msg, "lebt normal(%d)", bootCount);
      break;
    case BOOT_AKKU:
      sprintf(msg, "lebt AKKU(%d)", bootCount);
      break;
    case BOOT_WLAN:
      sprintf(msg, "lebt WLAN(%d)", bootCount);
      break;
    case BOOT_MQTT:
      sprintf(msg, "lebt MQTT (%d)", bootCount);
      break;
  }
  send_MSG_Other(msg, true);
  delay(50);
  client.loop();
  bootNachricht = BOOT_NORMAL;
  bootCount++;
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //  DHT22
  float T = 0;
  float F = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(&T, &F, NULL)) != SimpleDHTErrSuccess) {
    char nachricht[30];
    snprintf(nachricht, 29, "Fehler beim lesen von DHT22, err=%d", err);
    D_PRINTLN(nachricht); delay(2000);
    send_MSG_Other(nachricht, false);
    return;
  }
  D_PRINTF("DHT22: %f°C %f RH%%\n", T, F);
  send_MSG(DEVICEART1, T);
  delay(5);
  client.loop();
  send_MSG(DEVICEART2, F);
  delay(5);
  client.loop();

  // Photo-Wert
  int h = analogRead(RPHOTOPIN);
  D_PRINTF("Helligkeit: %d\n", h);
  send_MSG(DEVICEART3, h);
  delay(5);
  client.loop();

  // Level Akku
  float bat_level = analogRead(35) * 7.445f / 4096;
  D_PRINT((float)bat_level); D_PRINTLN(" V");
  send_MSG(DEVICEART4, bat_level);
  delay(5);
  client.loop();
  if (bat_level < 3.7) { // Akku leer, schlafen
    send_MSG_Other("Akku leer - schlafe", true);
    delay(5);
    client.loop();
    bootNachricht = BOOT_AKKU;
    esp_sleep_enable_timer_wakeup(ZEIT_ZW_NIEDRIGER_AKKU * uS_TO_S_FACTOR);
    delay(20);
    esp_deep_sleep_start();
  }

  D_PRINTLN("Going to sleep now");
  delay(50);
  esp_deep_sleep_start();
  D_PRINTLN("This will never be printed");
}

