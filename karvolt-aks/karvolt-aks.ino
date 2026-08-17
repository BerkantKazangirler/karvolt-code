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

struct TelemetriPaketi {
  unsigned long seq;
  unsigned long zaman;
  float hiz;
  float tmax;
  float voltaj;
  float akim;
  int soc;
  float kalanEnerji;
  float izolasyon;
  unsigned int maxHucre;
  unsigned int minHucre;
  ActiveStatus durum;
};

const unsigned long BMS_REQ_SOC_V_I  = 0x18900140;
const unsigned long BMS_RESP_SOC_V_I = 0x18904001;
const unsigned long BMS_REQ_HUCRE    = 0x18910140;
const unsigned long BMS_RESP_HUCRE   = 0x18914001;
const unsigned long BMS_REQ_KAPASITE = 0x18930140;
const unsigned long BMS_RESP_KAPASITE= 0x18934001;

BmsVerisi sonBmsVerisi = {0, 0, 0, true, 0, 0, false, 0, 0, 0, false};

unsigned long sonGonderimZamani = 0;
unsigned long sonNextionZamani = 0;
const unsigned long NEXTION_REFRESH_DELAY = 250; // Nextion ekran yenileme süresi (ms)

bool sdHazir = false;
char dosyaAdi[13]; // 8.3 format: "GGAA-N.CSV"

const int KUYRUK_KAPASITESI = 40;
TelemetriPaketi kuyruk[KUYRUK_KAPASITESI];
int kuyrukBas = 0;
int kuyrukSon = 0;
int kuyrukAdet = 0;
unsigned long paketSayaci = 0;

unsigned long sonBasariliAckZamani = 0;

void kuyrugaEkle(const TelemetriPaketi &p) {
  if (kuyrukAdet < KUYRUK_KAPASITESI) {
    kuyruk[kuyrukSon] = p;
    kuyrukSon = (kuyrukSon + 1) % KUYRUK_KAPASITESI;
    kuyrukAdet++;
    Serial.print("[KUYRUK] Paket yedeklendi. No: ");
    Serial.println(p.seq);
  } else {
    kuyrukBas = (kuyrukBas + 1) % KUYRUK_KAPASITESI;
    kuyruk[kuyrukSon] = p;
    kuyrukSon = (kuyrukSon + 1) % KUYRUK_KAPASITESI;
  }
}

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

ActiveStatus sistemDurumuHesapla(const SicaklikVerisi &sicaklik, float hizKmh, bool telemetriKopuk) {
  bool bmsTimeout = (millis() - sonBmsVerisi.sonVeriZamani > SEND_DELAY);

  if (sicaklik.sicaklik_hata || sonBmsVerisi.veri_hata || bmsTimeout || telemetriKopuk) {
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

const char* durumMetnineCevir(ActiveStatus durum, bool telemetriKopuk) {
  if (telemetriKopuk) return "BAGLANTI KOPUK (>60s)";

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
    return -127.0;
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
  HMI_SERIAL.print(".val=");
  HMI_SERIAL.print(deger);
  nextionKomutBitir();
}

void nextionMetinGonder(const char* obj, const String &metin) {
  HMI_SERIAL.print(obj);
  HMI_SERIAL.print(".txt=\"");
  HMI_SERIAL.print(metin);
  HMI_SERIAL.print("\"");
  nextionKomutBitir();
}

void csvSatiriYaz(const char* paketTipi, unsigned long seq, unsigned long zaman,
                  const SicaklikVerisi &sicaklik, float hizKmh, float v, float i, int soc,
                  float kalanEnerjiWh, float izolasyonDirenci, unsigned int maxH, unsigned int minH,
                  ActiveStatus durum) {
  if (!sdHazir) return;

  File csvDosya = SD.open(dosyaAdi, FILE_WRITE);
  if (!csvDosya) return;

  csvDosya.print(zaman);
  csvDosya.print(";");
  csvDosya.print(paketTipi);
  csvDosya.print(";");
  csvDosya.print(seq);
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
  csvDosya.print(v, 1);
  csvDosya.print(";");
  csvDosya.print(i, 1);
  csvDosya.print(";");
  csvDosya.print(soc);
  csvDosya.print(";");
  csvDosya.print(kalanEnerjiWh, 1);
  csvDosya.print(";");
  csvDosya.print(izolasyonDirenci, 0);
  csvDosya.print(";");
  csvDosya.print(maxH);
  csvDosya.print(";");
  csvDosya.println(minH);

  csvDosya.close();
}

void nextionGuncelle(const SicaklikVerisi &sicaklik, float hizKmh,
                      float kalanEnerjiWh, float izolasyonDirenci,
                      ActiveStatus durum, bool telemetriKopuk) {
  nextionSayiGonder("n0", (long)round(hizKmh));
  nextionSayiGonder("n1", (long)round(sicaklik.sicaklik_max));
  nextionSayiGonder("n2", (long)round(sonBmsVerisi.voltaj));
  nextionSayiGonder("x2", (long)round(sonBmsVerisi.akim * 10)); // vsc=1
  nextionMetinGonder("t1", String(sonBmsVerisi.soc) + "%");
  nextionSayiGonder("n4", (long)round(izolasyonDirenci));
  nextionSayiGonder("x0", (long)round(sonBmsVerisi.maxHucreVoltaj_mV / 10.0)); // vsc=2 ise 3.24 basar
nextionSayiGonder("x1", (long)round(sonBmsVerisi.minHucreVoltaj_mV / 10.0)); // vsc=2 ise 3.19 basar
  nextionSayiGonder("n5", (long)round(kalanEnerjiWh));
  nextionMetinGonder("t0", durumMetnineCevir(durum, telemetriKopuk));
}

bool gonderVeAckBekle(const String &mesaj, unsigned long seq, unsigned long timeoutMs) {
  while (LORA_SERIAL_P.available()) {
    LORA_SERIAL_P.read();
  }

  LORA_SERIAL_P.println(mesaj);

  unsigned long baslangic = millis();
  String ackBuf = "";

  while (millis() - baslangic < timeoutMs) {
    bmsCanKontrolEt(); // ACK beklerken CAN bus kaçırmamak için oku

    while (LORA_SERIAL_P.available()) {
      char c = (char)LORA_SERIAL_P.read();
      if (c == '\n') {
        ackBuf.trim();
        if (ackBuf.startsWith("ACK:")) {
          unsigned long gelenSeq = ackBuf.substring(4).toInt();
          if (gelenSeq == seq) {
            return true; // ACK doğrulandı
          }
        }
        ackBuf = "";
      } else if (c != '\r') {
        ackBuf += c;
        if (ackBuf.length() > 30) ackBuf = "";
      }
    }
  }
  return false; // ACK zaman aşımı
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  LORA_SERIAL_P.begin(LORA_SERIAL);
  HMI_SERIAL.begin(HMI_BAUD);

  pinMode(19, INPUT_PULLUP);

  pinMode(ROLE_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);

  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);
  digitalWrite(ALARM_PIN, LOW);
  digitalWrite(ROLE_PIN, HIGH);

  pinMode(CAN_INT_PIN, INPUT);

  sonBasariliAckZamani = millis();

  if (!SD.begin(SD_CS)) {
    Serial.println("SD kart baslatilamadi!");
    sdHazir = false;
  } else if (!dosyaAdiOlustur()) {
    sdHazir = false;
  } else {
    File f = SD.open(dosyaAdi, FILE_WRITE);
    if (f) {
      f.println("zaman_ms;paket_tipi;seq;durum;hiz_kmh;T1_C;T2_C;T3_C;Tmax_C;V_bat_V;I_bat_A;SOC;kalan_enerji_Wh;izolasyon_kOhm;max_hucre_mV;min_hucre_mV");
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
    digitalWrite(ROLE_PIN, LOW);
  }

  bmsCanKontrolEt();

  float hizKmh = hizKmhOku();
  float kalanEnerjiWh = kalanEnerjiHesapla();
  float izolasyonDirenci = izolasyonDirenciOku();
  bool telemetriKopuk = (millis() - sonBasariliAckZamani >= ((unsigned long)LOST_MAX * 1000UL));
  ActiveStatus mevcutDurum = sistemDurumuHesapla(sicaklik, hizKmh, telemetriKopuk);

  if (millis() - sonNextionZamani >= NEXTION_REFRESH_DELAY) {
    sonNextionZamani = millis();
    nextionGuncelle(sicaklik, hizKmh, kalanEnerjiWh, izolasyonDirenci, mevcutDurum, telemetriKopuk);
  }

  if (millis() - sonGonderimZamani >= SEND_DELAY) {
    sonGonderimZamani = millis();

    bmsTumIstekleriGonder();

    if (telemetriKopuk) {
      Serial.print("[UYARI - LOST_MAX] Telemetri baglantisi ");
      Serial.print(LOST_MAX);
      Serial.println(" saniyedir tamamen kesik!");
    }

    paketSayaci++;

    String mesaj = "TYPE:LIVE,SEQ:" + String(paketSayaci) +
                   ",TIME:" + String(millis()) +
                   ",SPD:"  + String(hizKmh, 1) +
                   ",Tmax:" + String(sicaklik.sicaklik_max, 1) +
                   ",V:"    + String(sonBmsVerisi.voltaj, 1);

    Serial.println("[GONDERILIYOR - CANLI] " + mesaj);

    bool ackGeldi = gonderVeAckBekle(mesaj, paketSayaci, 450);

    if (ackGeldi) {
      sonBasariliAckZamani = millis(); // ACK alındı, kopma zamanlayıcısını sıfırla

      Serial.print("[ACK ALINDI] Canli paket onaylandi. No: ");
      Serial.println(paketSayaci);

      csvSatiriYaz("CANLI", paketSayaci, millis(), sicaklik, hizKmh,
                   sonBmsVerisi.voltaj, sonBmsVerisi.akim, sonBmsVerisi.soc, kalanEnerjiWh,
                   izolasyonDirenci, sonBmsVerisi.maxHucreVoltaj_mV, sonBmsVerisi.minHucreVoltaj_mV, mevcutDurum);

      if (kuyrukAdet > 0) {
        TelemetriPaketi eskiPaket = kuyruk[kuyrukBas];
        delay(60); // RF modülü yarı-çift yönlü geçiş payı

        String tekrarMesaj = "TYPE:RETRY,SEQ:" + String(eskiPaket.seq) +
                             ",TIME:" + String(eskiPaket.zaman) +
                             ",SPD:"  + String(eskiPaket.hiz, 1) +
                             ",Tmax:" + String(eskiPaket.tmax, 1) +
                             ",V:"    + String(eskiPaket.voltaj, 1);

        Serial.println("[GONDERILIYOR - TEKRAR] " + tekrarMesaj);
        bool retryAck = gonderVeAckBekle(tekrarMesaj, eskiPaket.seq, 450);

        if (retryAck) {
          sonBasariliAckZamani = millis();
          Serial.print("[ACK ALINDI - TEKRAR] Gecmis paket onaylandi. No: ");
          Serial.println(eskiPaket.seq);
          kuyrukBas = (kuyrukBas + 1) % KUYRUK_KAPASITESI;
          kuyrukAdet--;

          csvSatiriYaz("TEKRAR", eskiPaket.seq, eskiPaket.zaman, sicaklik, eskiPaket.hiz,
                       eskiPaket.voltaj, eskiPaket.akim, eskiPaket.soc, eskiPaket.kalanEnerji,
                       eskiPaket.izolasyon, eskiPaket.maxHucre, eskiPaket.minHucre, eskiPaket.durum);
        } else {
          Serial.println("[KAYIP - TEKRAR] Gecmis paket iletilemedi, kuyrukta bekliyor.");
        }
      }
    } else {
      Serial.print("[KOPMA/KAYIP] ACK gelmedi! Paket kuyruga alindi: ");
      Serial.println(paketSayaci);

      TelemetriPaketi kayipPaket;
      kayipPaket.seq = paketSayaci;
      kayipPaket.zaman = millis();
      kayipPaket.hiz = hizKmh;
      kayipPaket.tmax = sicaklik.sicaklik_max;
      kayipPaket.voltaj = sonBmsVerisi.voltaj;
      kayipPaket.akim = sonBmsVerisi.akim;
      kayipPaket.soc = sonBmsVerisi.soc;
      kayipPaket.kalanEnerji = kalanEnerjiWh;
      kayipPaket.izolasyon = izolasyonDirenci;
      kayipPaket.maxHucre = sonBmsVerisi.maxHucreVoltaj_mV;
      kayipPaket.minHucre = sonBmsVerisi.minHucreVoltaj_mV;
      kayipPaket.durum = mevcutDurum;
      kuyrugaEkle(kayipPaket);

      csvSatiriYaz("KAYIP", paketSayaci, millis(), sicaklik, hizKmh,
                   sonBmsVerisi.voltaj, sonBmsVerisi.akim, sonBmsVerisi.soc, kalanEnerjiWh,
                   izolasyonDirenci, sonBmsVerisi.maxHucreVoltaj_mV, sonBmsVerisi.minHucreVoltaj_mV, mevcutDurum);
    }
  }
}