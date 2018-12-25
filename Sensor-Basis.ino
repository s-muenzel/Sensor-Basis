/* */

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef ESP32
#include <WiFi.h>
#endif
#include <PubSubClient.h>

#include <SimpleDHT.h>

#include "Zugangsinfo.h"

// Sensor DHT22
#define DHT22PIN 22
SimpleDHT22 dht22(DHT22PIN);


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
#define DEVICEART5	"Other" // Test: test mit variablen Infos


#define MQTT_MUSTER_BASIS		DEVICETYP "/" DEVICEID "/" DEVICEORT1 "/" DEVICEORT2 "/" DEVICEORT3 "/"

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

  int connect_trial_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    ++connect_trial_count;
    if(connect_trial_count > 15)
      esp_restart();
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
    if (client.connect(clientId.c_str(), device_user, device_pw)) {
      Serial.printf("connected: <%s>\n", clientId.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void send_MSG(const char* d_art, float wert) {
  snprintf (msg, MAX_NACHRICHT, "%f", wert);
  snprintf (_thema, MAX_THEMA, "%s%s", MQTT_MUSTER_BASIS, d_art);
  Serial.printf("Publish message: <%s>:<%s>", _thema, msg);
  if (client.publish(_thema, msg)) {
    Serial.println(" good");
  } else {
    Serial.println(" failed");
  }
}

void send_MSG_Other(char* inhalt) {
  snprintf (msg, MAX_NACHRICHT, "%s #%ld", inhalt, bootCount);
  Serial.printf("Publish message: <%s>:<%s>", MQTT_MUSTER_BASIS DEVICEART5, msg);
  if (client.publish(_thema, msg)) {
    Serial.println(" good");
  } else {
    Serial.println(" failed");
  }
}

//////////////////////////////////////////////////////
// Hauptprogramm

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  // Zur Begrüßung 3 mal flash
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    if (i < 2)
      delay(50);
  }

  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  print_wakeup_reason();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");
}

int value = 0;
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();

  float temperature = 0;
  float humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    char nachricht[30];
    snprintf(nachricht, 29, "Read DHT22 failed, err=%d", err);
    Serial.println(nachricht); delay(2000);
    send_MSG_Other(nachricht);
    return;
  }

  Serial.print("Sample OK: ");
  Serial.print((float)temperature); Serial.print(" *C, ");
  send_MSG(DEVICEART1, temperature);
  Serial.print((float)humidity); Serial.println(" RH%");
  send_MSG(DEVICEART2, humidity);

  float bat_level = analogRead(35)*3.3f/2048;
  Serial.print((float)bat_level); Serial.println(" V");
  send_MSG(DEVICEART4, bat_level);

  Serial.println("Going to sleep now");
  delay(50);
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}
