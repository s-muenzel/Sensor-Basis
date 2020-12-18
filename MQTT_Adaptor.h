#ifndef _INCLUDE_MQTT_ADATOR
#define _INCLUDE_MQTT_ADATOR

class MQTT_Adaptor {
  public:
    MQTT_Adaptor();
    void Beginn();
    void Ende();
    bool Verbunden();
    bool Verbinde(const char* ClientID, const char* User, const char* PW);
    bool Verbinde(const char* ClientID, const char* User, const char* PW, const char* WillTopic, int WillQoS, int WillRetain, const char* WillMessage);
    void Tick();
    bool Publish(const char* Thema, const char* Nachricht, bool retained = false, int qos = 0);
    int Status();
};
#endif // _INCLUDE_MQTT_ADAPTOR
