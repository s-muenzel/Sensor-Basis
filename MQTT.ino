/* */

#include "MQTT.h"
#include "MQTT_Adaptor.h"

MQTT_Adaptor __MQTT_Adaptor;

///////////////////////////////////////////////////
// MQTT Part

Mein_MQTT::Mein_MQTT() {
  sprintf(_client_ID, "E32__-%lX%lX", (long unsigned int)(0xffff & ESP.getEfuseMac()), (long unsigned int)(0xffff & (ESP.getEfuseMac() >> 32)));
}

void Mein_MQTT::Beginn() {
  __MQTT_Adaptor.Beginn();
}

bool Mein_MQTT::Verbinde() {
  int connect_trial_count = 0;
  D_PRINTF("Verbinde mit MQTT Broker als <%s> ", client_ID);
  while (!__MQTT_Adaptor.Verbunden()) {
    if (__MQTT_Adaptor.Verbinde(_client_ID, device_user, device_pw, MQTT_MUSTER_BASIS, 1, true, "tot")) {
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
  snprintf (msg, MAX_NACHRICHT, "%d,%d", bootCount, wert);
  return Sende(d_art, msg, false);
}

bool Mein_MQTT::Sende(const char* d_art, float wert) {
  char msg[MAX_NACHRICHT];
  snprintf (msg, MAX_NACHRICHT, "%d,%f", bootCount, wert);
  return Sende(d_art, msg, false);
}

bool Mein_MQTT::Sende(const char* d_art, const char* wert, bool retain) {
  strncpy(_msg, wert, MAX_NACHRICHT);
  snprintf (_thema, MAX_THEMA, "%s%s", MQTT_MUSTER_BASIS, d_art);
  D_PRINTF("Publish message: <%s>:<%s>", _thema, _msg);

  if (Verbinde() && __MQTT_Adaptor.Publish(_thema, _msg, retain, 1)) {
    D_PRINTLN(" good");
  } else {
    D_PRINTLN(" failed");
    return false;
  }
  __MQTT_Adaptor.Tick();
  delay(10);
  __MQTT_Adaptor.Tick();
  delay(10);
  return true;
}

void Mein_MQTT::Ende() {
  __MQTT_Adaptor.Ende();
}

