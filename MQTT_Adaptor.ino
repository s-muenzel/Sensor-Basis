#define USE_ARDUINO_MQTT
//#define USE_PUBSUBCLIENT

#include "MQTT_Adaptor.h"

#ifdef USE_PUBSUBCLIENT
#include <PubSubClient.h>
PubSubClient mqtt_client(__Wifi_Client);
#endif // USE_PUBSUBCLIENT
#ifdef USE_ARDUINO_MQTT
#include <MQTT.h>
#include <MqttClient.h>
MQTTClient mqtt_client;
#endif // USE_ARDUINO_MQTT


///////////////////////////////////////////////////
// MQTT Part als Adator Pattern

MQTT_Adaptor::MQTT_Adaptor() {
}

void MQTT_Adaptor::Beginn() {
#ifdef USE_PUBSUBBLIENT
  mqtt_client.setServer(mqtt_server, 1883);
#endif // USE_PUBSUBBLIENT
#ifdef USE_ARDUINO_MQTT
  mqtt_client.begin(mqtt_server, 1883, __Wifi_Client);
#endif // USE_ARDUINO_MQTT
}

void MQTT_Adaptor::Ende() {
  if (mqtt_client.connected())
    mqtt_client.disconnect();
}

bool MQTT_Adaptor::Verbunden() {
  return mqtt_client.connected();
}

bool MQTT_Adaptor::Verbinde(const char*ClientID, const char* User, const char* PW) {
  return mqtt_client.connect(ClientID, User, PW);
}

bool MQTT_Adaptor::Verbinde(const char*ClientID, const char* User, const char* PW, const char* WillTopic, int WillQoS, int WillRetain, const char*WillMessage) {
#ifdef USE_PUBSUBBLIENT
  mqtt_client.connect (ClientID, User, PW, WillTopic, WillQoS, WillRetain, WillMessage);
#endif // USE_PUBSUBBLIENT
#ifdef USE_ARDUINO_MQTT
  bool retval = mqtt_client.connect (ClientID, User, PW);
  mqtt_client.setWill(WillTopic, WillMessage, WillRetain, WillQoS);
  return retval;
#endif // USE_ARDUINO_MQTT
}

void MQTT_Adaptor::Tick() {
  yield();
  mqtt_client.loop();
  yield();
}

bool MQTT_Adaptor::Publish(const char *Thema, const char *Nachricht, bool retained, int qos) {
#ifdef USE_PUBSUBBLIENT
  yield();
  if (mqtt_client.publish(Thema, Nachricht, retained)) {
    delay(10);
    mqtt_client.loop();
    return true;
  } else
    return false;
#endif // USE_PUBSUBBLIENT
#ifdef USE_ARDUINO_MQTT
  yield();
  if (mqtt_client.publish(Thema, Nachricht, retained, qos)) {
    delay(10);
    mqtt_client.loop();
    return true;
  } else
    return false;
#endif // USE_ARDUINO_MQTT
}

int MQTT_Adaptor::Status() {
#ifdef USE_PUBSUBBLIENT
  return mqtt_client.state();
#endif // USE_PUBSUBBLIENT
#ifdef USE_ARDUINO_MQTT
  return mqtt_client.lastError();
#endif // USE_ARDUINO_MQTT
}
