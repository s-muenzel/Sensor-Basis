/* */

#ifndef INCLUDE_MQTT
#define INCLUDE_MQTT

#include <PubSubClient.h>

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

class Mein_MQTT {
public:
	Mein_MQTT();
	void Beginn();
	bool Verbinde();
	
	bool Sende(const char* d_art, int wert);
	bool Sende(const char* d_art, float wert);
	bool Sende(const char* d_art, const char* wert, bool retain = false);

private:
	PubSubClient client;
	char _msg[MAX_NACHRICHT];
	char _thema[MAX_THEMA]; // topic
	char client_ID[40];
};

#endif // INCLUDE_MQTT
