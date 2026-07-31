#include <math.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <mcp_can.h>
#include "config.h"

SoftwareSerial loraSerial(LORA_RX, LORA_TX);
MCP_CAN CAN(CAN_CS_PIN);

// 1kΩ direnç kullandığımız için 1000.0 yazıyoruz
const float R_REF = 1000.0;
const float R_0 = 10000.0;   // NTC'nin 25°C'deki direnci (10k)
const float T_0 = 298.15;    // 25°C (Kelvin)
const float B_VALUE = 3950;  // Standart Beta katsayısı

struct SicaklikVerisi {
  float sicaklik_sensor1;
  float sicaklik_sensor2;
  float sicaklik_sensor3;
  float sicaklik_max;
  bool sicaklik_hata;
};

struct BmsVerisi {
  float voltaj;      // V
  float akim;        // A
  int soc;           // %
  bool veri_hata;
};

// ---- BMS istek/cevap ID'leri (Daly CAN protokolü) ----
const unsigned long BMS_REQ_SOC_V_I  = 0x18900140;
const unsigned long BMS_RESP_SOC_V_I = 0x18904001;

BmsVerisi sonBmsVerisi = {0, 0, 0, true}; // başlangıçta hata varsayalım

float ntcOku(int pin) {
  int analogVal = analogRead(pin);

  if (analogVal > 0 && analogVal < 1023) {
    float vOut = analogVal * (5.0 / 1023.0);
    float rNtc = R_REF * ((5.0 / vOut) - 1.0);
    float tempK = 1.0 / ((1.0 / T_0) + (1.0 / B_VALUE) * log(rNtc / R_0));
    float tempC = tempK - 273.15;
    return tempC;
  } else {
    return -127.0; // Baglanti hatasi
  }
}

SicaklikVerisi sicaklikOku() {
  SicaklikVerisi v;
  v.sicaklik_sensor1 = ntcOku(SICAKLIK_DS1);
  v.sicaklik_sensor2 = ntcOku(SICAKLIK_DS2);
  v.sicaklik_sensor3 = ntcOku(SICAKLIK_DS3);

  v.sicaklik_hata = false;
  v.sicaklik_max = -127.0;

  if (v.sicaklik_sensor1 != -127.0) {
    if (v.sicaklik_sensor1 > v.sicaklik_max) v.sicaklik_max = v.sicaklik_sensor1;
  } else v.sicaklik_hata = true;

  if (v.sicaklik_sensor2 != -127.0) {
    if (v.sicaklik_sensor2 > v.sicaklik_max) v.sicaklik_max = v.sicaklik_sensor2;
  } else v.sicaklik_hata = true;

  if (v.sicaklik_sensor3 != -127.0) {
    if (v.sicaklik_sensor3 > v.sicaklik_max) v.sicaklik_max = v.sicaklik_sensor3;
  } else v.sicaklik_hata = true;

  return v;
}

void bmsIstekGonder() {
  byte req[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  CAN.sendMsgBuf(BMS_REQ_SOC_V_I, 1, 8, req); // 1 = extended frame
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  loraSerial.begin(LORA_SERIAL);

  pinMode(ROLE_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);

  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(ROLE_PIN, HIGH);

  pinMode(CAN_INT_PIN, INPUT);

  // Modülünüz 16 MHz kristale sahipse MCP_16MHZ kullanın:
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("MCP2515 CAN init OK");
  } else {
    Serial.println("MCP2515 CAN init FAIL");
  }
  CAN.setMode(MCP_NORMAL);
}

void bmsCanKontrolEt() {
  if (!digitalRead(CAN_INT_PIN)) {
    unsigned long rxId;
    byte len = 0;
    byte buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);

    // Kütüphanenin eklediği extended bitini temizliyoruz (0x1FFFFFFF maskesi ile):
    unsigned long temizId = rxId & 0x1FFFFFFF;

    Serial.print("CAN RX ID: 0x");
    Serial.print(rxId, HEX);
    Serial.print(" Data: ");
    for (int i = 0; i < len; i++) {
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Kontrolü temizlenmiş ID ile yapıyoruz
    if (temizId == BMS_RESP_SOC_V_I) {
      sonBmsVerisi.voltaj    = ((buf[0] << 8) | buf[1]) / 10.0;
      sonBmsVerisi.akim      = (((buf[4] << 8) | buf[5]) - 30000) / 10.0; // Byte 4 ve 5
      sonBmsVerisi.soc       = ((buf[6] << 8) | buf[7]) / 10;
      sonBmsVerisi.veri_hata = false;
    }
  }
}
void loop() {
  SicaklikVerisi sicaklik = sicaklikOku();

  // 1. İsteği Aktif Edin
  bmsIstekGonder();
  delay(50); // BMS cevabı için bekleme süresi

  // 2. Gelen Yanıtı Oku
  bmsCanKontrolEt();

  String mesaj = "Telemetri_Verisi:100"
                 ",T1:" + String(sicaklik.sicaklik_sensor1, 1) +
                 ",T2:" + String(sicaklik.sicaklik_sensor2, 1) +
                 ",T3:" + String(sicaklik.sicaklik_sensor3, 1) +
                 ",Tmax:" + String(sicaklik.sicaklik_max, 1) +
                 ",V:" + String(sonBmsVerisi.voltaj, 1) +
                 ",I:" + String(sonBmsVerisi.akim, 1) +
                 ",SOC:" + String(sonBmsVerisi.soc);

  loraSerial.write((uint8_t)0x00);
  loraSerial.write((uint8_t)0x01);
  loraSerial.write((uint8_t)0x17);
  loraSerial.println(mesaj);

  Serial.println("Paket Gonderildi: " + mesaj);

  if (sicaklik.sicaklik_max >= ALARM_MAX && sicaklik.sicaklik_max < SICAKLIK_MAX) {
    digitalWrite(ALARM_PIN, HIGH);
  } else if (sicaklik.sicaklik_max < ALARM_MAX) {
    digitalWrite(ALARM_PIN, LOW);
  } else if (sicaklik.sicaklik_max >= SICAKLIK_MAX) {
    digitalWrite(ROLE_PIN, LOW);
  }

  delay((TELEMETRY_SEND * 1000UL) - 50);
}