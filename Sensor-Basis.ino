/* Für Assuensensorstation
* Benötigt Bibliotheken
* SimpleDHT (Winlin)
* MQTT (Joel Gaehwiler)
* PubSubClient (Nick O'Leary) --> Alternative
* /// ACHTUNG: NSLOOKUP geht nicht, IP-Adresse MQTT-Server ist hard-coded
*/

#include <WiFi.h>

#include "MQTT.h"

//define FEUCHTESENSOR

//#define DEBUGING
//#define DEBUG_SERIAL
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
#define ZEIT_ZW_MESSUNGEN          5ll     /* wielange zwischen Sensor-Messungen(in Sekunden) */
#define ZEIT_ZW_NETZWERKFEHLER     5ll     /* bei WLAN oder MQTT Verbindungsproblemen (in Sekunden) */
#define ZEIT_ZW_NIEDRIGER_AKKU    10ll     /* wenn der Akku leer ist (in Sekunden) */
#else
#define ZEIT_ZW_MESSUNGEN        300ll     /* wielange zwischen Sensor-Messungen(in Sekunden) */
#define ZEIT_ZW_NETZWERKFEHLER     5ll     /* bei WLAN oder MQTT Verbindungsproblemen (in Sekunden) */
#define ZEIT_ZW_NIEDRIGER_AKKU  3600ll     /* wenn der Akku leer ist (in Sekunden) */
#endif

// Sensor DHT22
#define DHT22PIN 22
SimpleDHT22 __Dht22(DHT22PIN);
#define DHT22_PWR_PIN 27

// Sensor Photowiderstand
#define RPHOTOPIN 34

// Sensor Feuchtesensor
#define FEUCHTESENSORPIN 32
#define FEUCHTESENSORVDDPIN 27

// Akku Level
#define AKKU_NIEDRIG 3.7 			// Ab hier noch normales Operieren aber lange schlafen	
#define AKKU_LEER	 3.65			// Ab hier nix mehr tun ausser sofort wieder schlafen

WiFiClient	__Wifi_Client;
Mein_MQTT	__Mqtt;

// meine eigenen Boot-Codes (welcher Zustand vor dem letzten Deepsleep
enum boot_codes {
  BOOT_NORMAL,
  BOOT_AKKU,
  BOOT_WLAN,
  BOOT_MQTT
};

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR enum boot_codes bootNachricht = BOOT_NORMAL;

//////////////////////////////////////////////
// Deepsleep
#define uS_TO_S_FACTOR 1000000ll  /* Conversion factor for micro seconds to seconds */

void Schlafe(enum boot_codes Grund) {
  bootNachricht = Grund;
  switch (Grund) {
    case BOOT_NORMAL:
    default:
      D_PRINTLN("Schlafe bis zur nächsten Messung");
      esp_sleep_enable_timer_wakeup(ZEIT_ZW_MESSUNGEN * uS_TO_S_FACTOR);
      break;
    case BOOT_AKKU:
      D_PRINTLN("Akku alle, schlafe lange");
      esp_sleep_enable_timer_wakeup(ZEIT_ZW_NIEDRIGER_AKKU * uS_TO_S_FACTOR);
      break;
    case BOOT_WLAN:
      D_PRINTLN("Kein WLAN, noch'n Versuch");
      esp_sleep_enable_timer_wakeup(ZEIT_ZW_NETZWERKFEHLER * uS_TO_S_FACTOR);
      break;
    case BOOT_MQTT:
      D_PRINTLN("Kein MQTT, noch'n Versuch");
      esp_sleep_enable_timer_wakeup(ZEIT_ZW_NETZWERKFEHLER * uS_TO_S_FACTOR);
      break;
  }
  __Mqtt.Ende();
  delay(50);
  esp_deep_sleep_start();
}

/////////////////////////////////////////////////
// Normaler WiFi Part

bool setup_wifi() {
  D_PRINTF("Verbinde mit <%s> ", ssid);
  WiFi.begin(ssid, password);

  int connect_trial_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    D_PRINT(".");
    ++connect_trial_count;
    if (connect_trial_count == 8) {
      D_PRINTLN("Keine WLAN-Verbindung, Resette WiFi");
      WiFi.disconnect(true);
      delay(1);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      delay(1);
    }
    if (connect_trial_count > 15) {
      D_PRINTLN("Fehler: keine WLAN-Verbindung");
      return false;
    }
  }
  D_PRINTLN("ok");
  D_PRINT("ok, IP: ");
  D_PRINTLN(WiFi.localIP());
  return true;
}

//////////////////////////////////////////////////////
// Hauptprogramm

void setup() {
  D_BEGIN(115200);
  delay(1);
  // Level Akku - als erstes lesen. Falls zu niedrig, sofort wieder einschlafen
  float bat_level = analogRead(35) * 7.445f / 4096;
  D_PRINTF("Batteriespannung: %f\n",bat_level);
  if (bat_level < AKKU_LEER) { // Akku leer, schlafen
    Schlafe(BOOT_AKKU);
  }

#ifdef FEUCHTESENSOR
  pinMode(FEUCHTESENSORVDDPIN, OUTPUT); // Als erstes die Stromversorgung des Feuchtesensors anschalten
  digitalWrite(FEUCHTESENSORVDDPIN, HIGH);
#endif // FEUCHTESENSOR

#ifdef DEBUG_SERIAL
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, HIGH);
  // Zur Begrüßung 3 mal flash
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i < 2)
      delay(50);
  }
#endif

  if (!setup_wifi()) {
    Schlafe(BOOT_WLAN); // Schluss hier
  }

  __Mqtt.Beginn();
  if (!__Mqtt.Verbinde()) {
    Schlafe(bootNachricht); // Schluss hier
  }

  bootCount++;
  char msg[30];
  switch (bootNachricht) {
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
  __Mqtt.Sende("", msg, true);
  bootNachricht = BOOT_NORMAL;

  //  DHT22
  pinMode(DHT22_PWR_PIN, OUTPUT); // Als erstes die Stromversorgung des DHT22 anschalten
  digitalWrite(DHT22_PWR_PIN, HIGH);
#ifdef DEBUG_SERIAL
  digitalWrite(LED_BUILTIN, LOW);
#endif // DEBUG_SERIAL
  
  // Photo-Wert
  int h = analogRead(RPHOTOPIN);
  D_PRINTF("Helligkeit: %d\n", h);
  __Mqtt.Sende(DEVICEART3, h);


#ifdef FEUCHTESENSOR
  // Feuchtesensor
  int fs = analogRead(FEUCHTESENSORPIN);
  D_PRINTF("Wert Feuchesensor: %d\n", fs);
  __Mqtt.Sende(DEVICEART4, fs);
  digitalWrite(FEUCHTESENSORVDDPIN, LOW);
#endif // FEUCHTESENSOR


  //  DHT22, die 2.
  // jetzt messen
  delay(500);
  float T = 0;
  float F = 0;
  int err = SimpleDHTErrSuccess;
  int lese_versuche = 3;
  while (lese_versuche > 0) {
    if ((err = __Dht22.read2(&T, &F, NULL)) == SimpleDHTErrSuccess) {
      D_PRINTF("DHT22: %f°C %f RH%%\n", T, F);
      __Mqtt.Sende(DEVICEART1, T);
      __Mqtt.Sende(DEVICEART2, F);
      break;
    }
    lese_versuche--;
    delay(500);
  }
  digitalWrite(DHT22_PWR_PIN, LOW); // Strom sparen
#ifdef DEBUG_SERIAL
  digitalWrite(LED_BUILTIN, HIGH);
#endif // DEBUG_SERIAL
  if (lese_versuche == 0) {
    char nachricht[50];
    snprintf(nachricht, 29, "Fehler beim lesen von DHT22, err=%d", err);
    D_PRINTLN(nachricht);
    __Mqtt.Sende("", nachricht, false);
  }
  // Level Akku
  D_PRINTF("Akkuspannung: %f V\n", (float)bat_level);
  __Mqtt.Sende(DEVICEART5, bat_level);
  if (bat_level < AKKU_NIEDRIG) { // Akku leer, schlafen
    __Mqtt.Sende("", "Akku leer - schlafe", true);
    D_PRINTF("Akku leer, gehe jetzt schlafen %lld Sekunden schlafen\n", ZEIT_ZW_NIEDRIGER_AKKU);
    Schlafe(BOOT_AKKU);
  }

  delay(50); yield();
  D_PRINTF("gehe jetzt schlafen %lld Sekunden schlafen\n", ZEIT_ZW_MESSUNGEN);
  Schlafe(BOOT_NORMAL);
}

void loop() {
}
