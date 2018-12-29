/* */

#include "MQTT.h"

///////////////////////////////////////////////////
// MQTT Part

Mein_MQTT::Mein_MQTT() {
  sprintf(client_ID, "E32__-%lX%lX", (long unsigned int)(0xffff & ESP.getEfuseMac()), (long unsigned int)(0xffff & (ESP.getEfuseMac() >> 32)));
}

void Mein_MQTT::Beginn() {
  client.setServer(mqtt_server, 1883);
  client.setClient(__WifiClient);
}

bool Mein_MQTT::Verbinde() {
  int connect_trial_count = 0;
  D_PRINTF("Verbinde mit MQTT Broker als <%s> ", client_ID);
  while (!client.connected()) {
    if (client.connect(client_ID, device_user, device_pw, MQTT_MUSTER_BASIS, 0, true, "tot", true)) {
      D_PRINTLN("ok");
      return true;
    } else {
      D_PRINTF("geht nicht, rc=%d\n", client.state());
      ++connect_trial_count;
      if (connect_trial_count > 10) {
        D_PRINTLN("Fehler: kein MQTT");
        return false;
      }
      delay(1000);
    }
  }
  return true; // kann eigentlich nicht vorkommen
}

bool Mein_MQTT::Sende(const char* d_art, int wert) {
  char msg[MAX_NACHRICHT];
  snprintf (msg, MAX_NACHRICHT, "%d", wert);
  return Sende(d_art, msg, false);
}

bool Mein_MQTT::Sende(const char* d_art, float wert) {
  char msg[MAX_NACHRICHT];
  snprintf (msg, MAX_NACHRICHT, "%f", wert);
  return Sende(d_art, msg, false);
}

bool Mein_MQTT::Sende(const char* d_art, const char* wert, bool retain) {
  strncpy(_msg, wert, MAX_NACHRICHT);
  snprintf (_thema, MAX_THEMA, "%s%s", MQTT_MUSTER_BASIS, d_art);
  D_PRINTF("Publish message: <%s>:<%s>", _thema, _msg);
  
  if (Verbinde() && client.publish(_thema, _msg, retain)) {
    D_PRINTLN(" good");
  } else {
    D_PRINTLN(" failed");
    return false;
  }
  delay(100);
  if (!client.loop()) {
    D_PRINTF("Sende: loop failed: %d", client.state());
    return false;
  }
  delay(100);
  return true;
}

