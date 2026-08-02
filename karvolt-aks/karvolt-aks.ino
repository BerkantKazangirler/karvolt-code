#include <math.h>
#include <string.h>
#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>
#include "config.h"

MCP_CAN CAN(CAN_CS_PIN);

const float R_REF = 1000.0;
const float R_0 = 10000.0;
const float T_0 = 298.15;
const float B_VALUE = 3950;

struct SicaklikVerisi {
  float sicaklik_sensor1;
  float sicaklik_sensor2;
  float sicaklik_sensor3;
  float sicaklik_max;
  bool sicaklik_hata;
};

struct BmsVerisi {
  float voltaj;                    // V
  float akim;                      // A
  int soc;                         // %
  bool veri_hata;
  unsigned long sonVeriZamani;     // BMS zaman aşımı takibi
  unsigned long kalanKapasite_mAh; // Daly 0x93
  bool kapasite_gecerli;           
  int sarjDurumu;                  // 0x93 Byte0: 0=duruyor, 1=sarj, 2=desarj
  unsigned int maxHucreVoltaj_mV;  // 0x91
  unsigned int minHucreVoltaj_mV;  // 0x91
  bool hucre_gecerli;              
};

const unsigned long BMS_REQ_SOC_V_I  = 0x18900140;
const unsigned long BMS_RESP_SOC_V_I = 0x18904001;

const unsigned long BMS_REQ_HUCRE    = 0x18910140;
const unsigned long BMS_RESP_HUCRE   = 0x18914001;

const unsigned long BMS_REQ_KAPASITE = 0x18930140;
const unsigned long BMS_RESP_KAPASITE= 0x18934001;

BmsVerisi sonBmsVerisi = {0, 0, 0, true, 0, 0, false, 0, 0, 0, false};

unsigned long sonGonderimZamani = 0;
bool sdHazir = false;
char dosyaAdi[13]; // 8.3 format: "GGAA-N.CSV"

float hizKmhOku() {
  return HIZ_KMH_SABIT;
}

float izolasyonDirenciOku() {
  return IZOLASYON_DIRENC_KOHM_SABIT;
}

float kalanEnerjiHesapla() {
  if (sonBmsVerisi.kapasite_gecerli) {
    return (sonBmsVerisi.kalanKapasite_mAh / 1000.0) * sonBmsVerisi.voltaj;
  } else {
    return (BATARYA_PAKET_KAPASITE_WH * sonBmsVerisi.soc) / 100.0;
  }
}

ActiveStatus sistemDurumuHesapla(const SicaklikVerisi &sicaklik, float hizKmh) {
  bool bmsTimeout = (millis() - sonBmsVerisi.sonVeriZamani > 3000);

  if (sicaklik.sicaklik_hata || sonBmsVerisi.veri_hata || bmsTimeout) {
    return STATE_ERROR;
  }
  if (sicaklik.sicaklik_max >= SICAKLIK_MAX || hizKmh >= HIZ_KRITIK_KMH) {
    return STATE_CRITICAL;
  }
  if (sonBmsVerisi.sarjDurumu == 1) {
    return STATE_CHARGING;
  }
  if (sonBmsVerisi.sarjDurumu == 2 || hizKmh > 0.5) {
    return STATE_RUNNING;
  }
  return STATE_IDLE;
}

const char* durumMetnineCevir(ActiveStatus durum) {
  switch (durum) {
    case STATE_IDLE:     return "SISTEM NORMAL";
    case STATE_RUNNING:  return "SURUS / DESARJ";
    case STATE_CHARGING: return "SARJ OLUYOR";
    case STATE_CRITICAL: return "KRITIK DURUM!";
    case STATE_ERROR:    return "SENSOR/BMS HATASI";
    default:             return "BILINMEYEN";
  }
}

void tarihStringOlustur(char* buffer) {
  const char* aylar[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  const char* d = __DATE__;

  char ayStr[4] = {d[0], d[1], d[2], '\0'};
  int gun = ((d[4] == ' ') ? 0 : (d[4] - '0')) * 10 + (d[5] - '0');

  int ay = 1;
  for (int i = 0; i < 12; i++) {
    if (strncmp(ayStr, aylar[i], 3) == 0) {
      ay = i + 1;
      break;
    }
  }

  sprintf(buffer, "%02d%02d", gun, ay);
}

bool dosyaAdiOlustur() {
  char tarih[5];
  tarihStringOlustur(tarih);

  for (int n = 1; n <= 999; n++) {
    sprintf(dosyaAdi, "%s-%d.CSV", tarih, n);
    if (!SD.exists(dosyaAdi)) {
      return true;
    }
  }
  return false;
}

float ntcOku(int pin) {
  int analogVal = analogRead(pin);

  if (analogVal > 0 && analogVal < 1023) {
    float vOut = analogVal * (5.0 / 1023.0);
    float rNtc = R_REF * ((5.0 / vOut) - 1.0);
    float tempK = 1.0 / ((1.0 / T_0) + (1.0 / B_VALUE) * log(rNtc / R_0));
    return tempK - 273.15;
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

void bmsTumIstekleriGonder() {
  byte req[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  
  CAN.sendMsgBuf(BMS_REQ_SOC_V_I, 1, 8, req);
  delay(10);
  
  CAN.sendMsgBuf(BMS_REQ_KAPASITE, 1, 8, req);
  delay(10);
  
  CAN.sendMsgBuf(BMS_REQ_HUCRE, 1, 8, req);
}

void bmsCanKontrolEt() {
  while (!digitalRead(CAN_INT_PIN)) {
    unsigned long rxId;
    byte len = 0;
    byte buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);

    unsigned long temizId = rxId & 0x1FFFFFFF;

    if (temizId == BMS_RESP_SOC_V_I) {
      sonBmsVerisi.voltaj        = ((buf[0] << 8) | buf[1]) / 10.0;
      sonBmsVerisi.akim          = (((buf[4] << 8) | buf[5]) - 30000) / 10.0;
      sonBmsVerisi.soc           = ((buf[6] << 8) | buf[7]) / 10;
      sonBmsVerisi.veri_hata     = false;
      sonBmsVerisi.sonVeriZamani = millis();
    } else if (temizId == BMS_RESP_KAPASITE) {
      sonBmsVerisi.sarjDurumu        = buf[0];
      sonBmsVerisi.kalanKapasite_mAh = ((unsigned long)buf[4] << 24) | ((unsigned long)buf[5] << 16) |
                                       ((unsigned long)buf[6] << 8)  | buf[7];
      sonBmsVerisi.kapasite_gecerli  = true;
      sonBmsVerisi.sonVeriZamani     = millis();
    } else if (temizId == BMS_RESP_HUCRE) {
      sonBmsVerisi.maxHucreVoltaj_mV = ((unsigned int)buf[0] << 8) | buf[1];
      sonBmsVerisi.minHucreVoltaj_mV = ((unsigned int)buf[3] << 8) | buf[4];
      sonBmsVerisi.hucre_gecerli     = true;
      sonBmsVerisi.sonVeriZamani     = millis();
    }
  }
}

void nextionKomutBitir() {
  HMI_SERIAL.write(0xFF);
  HMI_SERIAL.write(0xFF);
  HMI_SERIAL.write(0xFF);
}

void nextionSayiGonder(const char* obj, long deger) {
  HMI_SERIAL.print(obj);
  HMI_SERIAL.print("=");
  HMI_SERIAL.print(deger);
  nextionKomutBitir();
}

void nextionMetinGonder(const char* obj, const String &metin) {
  HMI_SERIAL.print(obj);
  HMI_SERIAL.print("=\"");
  HMI_SERIAL.print(metin);
  HMI_SERIAL.print("\"");
  nextionKomutBitir();
}

void csvSatiriYaz(const SicaklikVerisi &sicaklik, float hizKmh, float kalanEnerjiWh, float izolasyonDirenci, ActiveStatus durum) {
  if (!sdHazir) return;

  File csvDosya = SD.open(dosyaAdi, FILE_WRITE);
  if (!csvDosya) {
    Serial.println("CSV dosyasi acilamadi");
    return;
  }

  csvDosya.print(millis());
  csvDosya.print(";");
  csvDosya.print((uint8_t)durum);
  csvDosya.print(";");
  csvDosya.print(hizKmh, 1);
  csvDosya.print(";");
  csvDosya.print(sicaklik.sicaklik_sensor1, 1);
  csvDosya.print(";");
  csvDosya.print(sicaklik.sicaklik_sensor2, 1);
  csvDosya.print(";");
  csvDosya.print(sicaklik.sicaklik_sensor3, 1);
  csvDosya.print(";");
  csvDosya.print(sicaklik.sicaklik_max, 1);
  csvDosya.print(";");
  csvDosya.print(sonBmsVerisi.voltaj, 1);
  csvDosya.print(";");
  csvDosya.print(sonBmsVerisi.akim, 1);
  csvDosya.print(";");
  csvDosya.print(sonBmsVerisi.soc);
  csvDosya.print(";");
  csvDosya.print(kalanEnerjiWh, 1);
  csvDosya.print(";");
  csvDosya.print(izolasyonDirenci, 0);
  csvDosya.print(";");
  csvDosya.print(sonBmsVerisi.maxHucreVoltaj_mV);
  csvDosya.print(";");
  csvDosya.println(sonBmsVerisi.minHucreVoltaj_mV);

  csvDosya.close();
}

void nextionGuncelle(const SicaklikVerisi &sicaklik, float hizKmh,
                      float kalanEnerjiWh, float izolasyonDirenci,
                      ActiveStatus durum) {
  nextionSayiGonder("n0", (long)round(hizKmh));
  nextionSayiGonder("n1", (long)round(sicaklik.sicaklik_max));
  nextionSayiGonder("n2", (long)round(sonBmsVerisi.voltaj));
  nextionSayiGonder("x2", (long)round(sonBmsVerisi.akim * 10)); // vsc=1
  nextionMetinGonder("t1", String(sonBmsVerisi.soc) + "%");
  nextionSayiGonder("n4", (long)round(izolasyonDirenci));
  nextionSayiGonder("x0", (long)sonBmsVerisi.maxHucreVoltaj_mV); // mV, vsc=3
  nextionSayiGonder("x1", (long)sonBmsVerisi.minHucreVoltaj_mV); // mV, vsc=3
  nextionSayiGonder("n5", (long)round(kalanEnerjiWh));
  nextionMetinGonder("t0", durumMetnineCevir(durum));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  LORA_SERIAL_P.begin(LORA_SERIAL);
  HMI_SERIAL.begin(HMI_BAUD);

  pinMode(ROLE_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);

  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(ROLE_PIN, HIGH);

  pinMode(CAN_INT_PIN, INPUT);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD kart baslatilamadi!");
    sdHazir = false;
  } else if (!dosyaAdiOlustur()) {
    sdHazir = false;
  } else {
    File f = SD.open(dosyaAdi, FILE_WRITE);
    if (f) {
      f.println("zaman_ms;durum;hiz_kmh;T1_C;T2_C;T3_C;Tmax_C;V_bat_V;I_bat_A;SOC;kalan_enerji_Wh;izolasyon_kOhm;max_hucre_mV;min_hucre_mV");
      f.close();
      sdHazir = true;
      Serial.print("CSV dosyasi olusturuldu: ");
      Serial.println(dosyaAdi);
    } else {
      sdHazir = false;
    }
  }

  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("MCP2515 CAN init OK");
  } else {
    Serial.println("MCP2515 CAN init FAIL");
  }
  CAN.setMode(MCP_NORMAL);
}

void loop() {
  SicaklikVerisi sicaklik = sicaklikOku();

  if (sicaklik.sicaklik_max >= ALARM_MAX && sicaklik.sicaklik_max < SICAKLIK_MAX) {
    digitalWrite(ALARM_PIN, HIGH);
  } else if (sicaklik.sicaklik_max < ALARM_MAX) {
    digitalWrite(ALARM_PIN, LOW);
  }

  if (sicaklik.sicaklik_max >= SICAKLIK_MAX) {
    digitalWrite(ROLE_PIN, LOW); // Acil kesme
  }

  bmsCanKontrolEt();

  if (millis() - sonGonderimZamani >= SEND_DELAY) {
    sonGonderimZamani = millis();

    bmsTumIstekleriGonder();

    float hizKmh = hizKmhOku();
    float kalanEnerjiWh = kalanEnerjiHesapla();
    float izolasyonDirenci = izolasyonDirenciOku();
    
    ActiveStatus mevcutDurum = sistemDurumuHesapla(sicaklik, hizKmh);

    String mesaj = "Telemetri_Verisi:100"
                   ",TIME:" + String(millis()) +
                   ",SPD:"  + String(hizKmh, 1) +
                   ",Tmax:" + String(sicaklik.sicaklik_max, 1) +
                   ",V:"    + String(sonBmsVerisi.voltaj, 1);

    LORA_SERIAL_P.write((uint8_t)0x00);
    LORA_SERIAL_P.write((uint8_t)0x01);
    LORA_SERIAL_P.write((uint8_t)0x17);
    LORA_SERIAL_P.println(mesaj);

    Serial.println("Paket Gonderildi: " + mesaj);

    csvSatiriYaz(sicaklik, hizKmh, kalanEnerjiWh, izolasyonDirenci, mevcutDurum);
    
    nextionGuncelle(sicaklik, hizKmh, kalanEnerjiWh, izolasyonDirenci, mevcutDurum);
  }
}