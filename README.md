# Sensor-Basis

Erster Prototyp für einen Sensor

Requirements:
- Publiziert seine Werte über mqtt an das NAS

- T, F über einen DHT22 (AM2...)

- Helligkeit über einen Photowiderstand

- Batteriestand 

Basis: ESP32


MQTT Regeln:
Device-Typ	Device-ID	Ort						Art	Werte
						1			2		3		
Sensor		InnenTuF	EG			WZ			T	Temperatur, in C, float
Sensor		InnenTuF	EG			WZ			F	Rel. Feuchte in %, float
Actor						
