#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "config.h"

SoftwareSerial loraSerial(LORA_RX, LORA_TX); 

OneWire oneWire1(SICAKLIK_DS1);
OneWire oneWire2(SICAKLIK_DS2);
OneWire oneWire3(SICAKLIK_DS3);

DallasTemperature sicaklik_sensor1(&oneWire1);
DallasTemperature sicaklik_sensor2(&oneWire2);
DallasTemperature sicaklik_sensor3(&oneWire3);

struct SicaklikVerisi {
  float sicaklik_sensor1;
  float sicaklik_sensor2;
  float sicaklik_sensor3;
  float sicaklik_ortalama;
  bool sicaklik_hata;
};

void sicaklikBaslat() {
  sicaklik_sensor1.begin();
  sicaklik_sensor2.begin();
  sicaklik_sensor3.begin();
}

SicaklikVerisi sicaklikOku() {
  sicaklik_sensor1.requestTemperatures();
  sicaklik_sensor2.requestTemperatures();
  sicaklik_sensor3.requestTemperatures();

  SicaklikVerisi v;
  v.sicaklik_sensor1 = sicaklik_sensor1.getTempCByIndex(0);
  v.sicaklik_sensor2 = sicaklik_sensor2.getTempCByIndex(0);
  v.sicaklik_sensor3 = sicaklik_sensor3.getTempCByIndex(0);

  int validCount = 0;
  float total = 0;
  v.sicaklik_hata = false;

  if (v.sicaklik_sensor1 != DEVICE_DISCONNECTED_C) { total += v.sicaklik_sensor1; validCount++; } else v.sicaklik_hata = true;
  if (v.sicaklik_sensor2 != DEVICE_DISCONNECTED_C) { total += v.sicaklik_sensor2; validCount++; } else v.sicaklik_hata = true;
  if (v.sicaklik_sensor3 != DEVICE_DISCONNECTED_C) { total += v.sicaklik_sensor3; validCount++; } else v.sicaklik_hata = true;

  v.sicaklik_ortalama = (validCount > 0) ? (total / validCount) : -127.0;
  return v;
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(ROLE_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);

  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);

  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);

  sicaklikBaslat();

  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(ROLE_PIN, HIGH);

  Serial.println("kıyamet bugündür ya ümmeti muhammed");
}

void loop() {
  SicaklikVerisi sicaklik = sicaklikOku();

  if (sicaklik_ortalama => ALARM_MAX && sicaklik_ortalama < SICAKLIK_MAX) {
    digitalWrite(ALARM_PIN, HIGH); // Alarm Aç
  } else if (sicaklik_ortalama < ALARM_MAX) {
    digitalWrite(ALARM_PIN, LOW); // Alarm Kapat
  } else if (sicaklik_ortalama => SICAKLIK_MAX) {
    digitalWrite(ROLE_PIN, LOW); // Kapat
  }

  delay(1000);
}