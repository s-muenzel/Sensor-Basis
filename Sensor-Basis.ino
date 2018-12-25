/* */

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include <PubSubClient.h>

#include "Zugangsinfo.h"



// MQTT Muster Daten:
#define DEVICETYP	"sensor"
#define DEVICEID	"WZTuF"
#define DEVICEORT1	"EG"
#define DEVICEORT2	"WZ"
#define DEVICEORT3	""
#define DEVICEART1	"T" // 1. Sensorwert: Temperatur
#define DEVICEART2	"F" // 2. Sensorwert: Rel. Feuchte
#define DEVICEART3	"H" // 3. Sensorwert: Helligkeit
#define DEVICEART4	"B" // 4. Sensorwert: Batteriestatus
#define DEVICEART5	"Other" // Test: test mit variablen Infos


#define MQTT_MUSTER_BASIS		DEVICETYP DEVICEID DEVICEORT1 DEVICEORT2 DEVICEORT3

#define MAX_NACHRICHT   40
#define MAX_THEMA       55 // MQTT Topic (siehe oben)

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[MAX_NACHRICHT];

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR char _nachricht[20]; // Anfang der Nachricht
RTC_DATA_ATTR char _thema[MAX_THEMA]; // topic

//////////////////////////////////////////////
// Deepsleep Part
/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}

/////////////////////////////////////////////////
// Normaler WiFi Part

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

///////////////////////////////////////////////////
// MQTT Part

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message arrived [%s] ", topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
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
    Serial.printf("Attempting MQTT connection <%s> ", clientId.c_str());
    // Attempt to connect
    if (client.connect(clientId.c_str(), DEVICETYP, )) {
      Serial.printf("connected: <%s>\n", clientId.c_str());
      // resubscribe if needed
      if (_lausche) {
        Serial.printf("subscribing to <%s> ", _thema);
        if (client.subscribe(_thema)) {
          Serial.println("erfolgreich");
        } else {
          Serial.println("nicht erfolgreich");
        }
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void send_MSG(long now) {
  lastMsg = now;
  ++value; ++bootCount;
  snprintf (msg, MAX_NACHRICHT, "%s #%ld-%ld", _nachricht, value, bootCount);
  Serial.printf("Publish message: <%s>:<%s>", _thema, msg);
  if (client.publish(_thema, msg)) {
    Serial.printf(" good: <%s>:<%s>\n", _thema, msg);
  } else {
    Serial.printf(" failed: <%s>:<%s>\n", _thema, msg);
  }
}

//////////////////////////////////////////////////////
// Hauptprogramm

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (bootCount == 0) {
    strcpy(_thema, "meinKanal");
#ifdef ESP8266
    strcpy(_nachricht, "mein ESP8266");
#endif
#ifdef ESP32
    strcpy(_nachricht, "mein ESP32__");
#endif
  }
  print_wakeup_reason();
  print_wakeup_touchpad();
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");
                 
  //Setup interrupt on Touch Pad 3 (GPIO15)
  touchAttachInterrupt(T3, TouchCallback, Threshold);
  //Configure Touchpad as wakeup source
  esp_sleep_enable_touchpad_wakeup();


}

void loop() {

  if (Serial.available() > 0) {
    // read the incoming byte:
    char _c  = Serial.read(); // Kommando
    String _msg = Serial.readStringUntil('\n');
    _msg.trim();
    int _len = _msg.length();
    if (_len > 15)
      return;
    switch (_c) {
      case 's': // _s_enden
        strncpy(_nachricht, _msg.c_str(), 20);
        _sende = 1;
        Serial.printf("einmal senden: <%s>\n", _nachricht);
        break;
      case 'r': // _r_egelmäßig Senden
        strncpy(_nachricht, _msg.c_str(), 20);
        _sende = -1;
        Serial.printf("dauernd senden: <%s>\n", _nachricht);
        break;
      case 'a': // senden _a_nhalten
        _sende = 0;
        Serial.printf("senden aufhoeren\n");
        break;
      case 'l': // _l_auschen (subscribe)
        _lausche = true;
        if (client.connected()) {
          client.loop();
          client.subscribe(_thema);
          client.loop();
        }
        Serial.printf("subscribe to: <%s>\n", _thema);
        break;
      case 'w': // _w_eghören (unsubscribe)
        _lausche = false;
        if (client.connected()) {
          client.loop();
          client.unsubscribe(_thema);
          client.loop();
        }
        Serial.printf("unsubscribe\n");
        break;
      case 't': // _t_opic
        if (_lausche) {
          client.loop();
          client.unsubscribe(_thema);
          client.loop();
        }
        strncpy (_thema, _msg.c_str(), MAX_THEMA);
        if (_lausche) {
          client.loop();
          client.subscribe(_thema);
          client.loop();
        }
        Serial.printf("Neues Thema: <%s>\n", _thema);
        break;
      default: // ??? - Anleitung schicken
        Serial.println("Unbekannter Befehl, mögliche Befehle sind:");
        Serial.println("sxxxx: einmal senden, Botschaft xxxx");
        Serial.println("rxxxx: regelmäßig senden, Botschaft xxxx");
        Serial.println("a: senden aufhören");
        Serial.println("l: subscribe");
        Serial.println("w: unsubscribe");
        Serial.println("txxxx: topic festlegen");
        break;
    }
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (_sende == 1) {
    send_MSG(now);
    _sende = 0;
  }
  if ((_sende == -1) && (now - lastMsg > 2000)) {
    send_MSG(now);
  }
  if (value > 3) {
    Serial.println("Going to sleep now");
    delay(50);
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
  }
}