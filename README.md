# KarVolt Projesi# 🏎️ KARVOLT AKS - Araç Kontrol & Telemetri Sistemi

![Arduino](https://img.shields.io/badge/Board-Arduino%20Mega%202560-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![CAN Bus](https://img.shields.io/badge/Protocol-CAN%20Bus%20(29--bit)-00599C?style=for-the-badge)
![LoRa](https://img.shields.io/badge/Wireless-LoRa%20E32-FF6F00?style=for-the-badge)
![Competition](https://img.shields.io/badge/TEKNOFEST-Efficiency%20Challenge-red?style=for-the-badge)

KARVOLT Elektrikli Araç Takımı için geliştirilmiş **Araç Kontrol Sistemi (AKS)** ve **Telemetri** merkezi yazılım deposudur.

Bu sistem; araç üzerindeki **Daly BMS** ile CAN Bus hattı üzerinden haberleşerek batarya parametrelerini izler, yarış kural kitapçığına uygun minimal verileri **LoRa** üzerinden yer istasyonuna aktarır, detaylı tüm sensör ve batarya verilerini **SD Karta** loglar ve sürücüyü **Nextion HMI** ekranı üzerinden anlık bilgilendirir.

---

## 📋 Proje Hakkında

Araç Kontrol Sistemi, elektrikli aracın ana işlem ve güvenlik birimidir. `millis()` tabanlı, bloklanmayan modüler mimarisi sayesinde tüm alt sistemlerin eşzamanlı ve kesintisiz çalışmasını sağlar.

* **Daly BMS CAN Bus Entegrasyonu:** Extended CAN (29-bit ID) protokolü ile paket voltajı, akım, SOC, hücre min/maks voltajları ve kapasite verilerinin anlık takibi.
* **Kural Kitapçığı Uyumlu Telemetri:** LoRa airtime süresini optimize eden, kanal çakışmalarını önleyen minimalist veri paketi iletimi.
* **Kara Kutu (SD Loglama):** Yarış sonrası teknik analiz ve optimizasyon için hücre bazlı voltaj ve sıcaklık verilerinin tarih tabanlı (`GGAA-N.CSV`) olarak saklanması.
* **HMI Sürücü Ekranı:** Nextion dokunmatik ekran üzerinden araç hızı, sıcaklık, batarya durumu ve sistem mesajlarının sürücüye iletilmesi.
* **Donanımsal ve Yazılımsal Güvenlik:** Limit aşımı durumlarında (yüksek sıcaklık, BMS iletişim kopukluğu) devreye giren sesli alarm ve acil durum batarya kesme rölesi.

---

## 👥 Ekip ve Görev Dağılımı

KARVOLT Araç Kontrol ve Telemetri ekibi donanım, gömülü yazılım ve veri işleme süreçlerini disiplinli bir iş bölümüyle sürdürmektedir:

* **Yazılım & AKS Lideri:** *Berkant Kazangirler*
  * *Sorumluluklar:* Sistem mimarisinin tasarlanması, Arduino Mega 2560 gömülü yazılımı, Daly BMS Extended CAN Bus protokolü entegrasyonu, State Machine yapısı, bellek/RAM optimizasyonu ve kritik güvenlik kesme mekanizmaları.

* **Telemetri Ekibi:**
  * **Gürkan Yılmaz** — Telemetri Donanım & Kablosuz İletim Sistemleri (LoRa RF modül kalibrasyonu, anten optimizasyonları ve donanımsal haberleşme hatları)
  * **Betül Kuşkaya** — Telemetri Veri İşleme & Yer İstasyonu Arayüzü (Yer istasyonu yazılımı, anlık veri görselleştirme ve telemetri paket çözücüler)

---

## 🔌 Donanım ve Pin Bağlantı Haritası

Sistem merkezinde **Arduino Mega 2560** mikrodenetleyicisi yer almaktadır. Bağlantı mimarisi aşağıdaki gibidir:

| Bileşen / Modül | Modül Tipi | Arduino Mega Pin | Protokol / Açıklama |
| :--- | :--- | :--- | :--- |
| **MCP2515** | CAN Bus Modülü | CS: `53`, INT: `21` | SPI (16MHz Crystal, 500Kbps) |
| **LoRa E32** | Telemetri Vericisi | RX1: `19`, TX1: `18` | Hardware `Serial1` (M0: `4`, M1: `5`) |
| **Nextion HMI** | Sürücü Ekranı | RX2: `17`, TX2: `16` | Hardware `Serial2` |
| **SD Kart Modülü** | Veri Loglayıcı | CS: `10` | SPI Protokolü |
| **NTC Sensörler** | Sıcaklık Ölçümü | `A0`, `A1`, `A2` | 3 Noktalı Batarya Sıcaklık Takibi |
| **Güvenlik Rölesi** | Acil Kesici | `Pin 7` | Power Relay (Active LOW) |
| **Sesli Alarm** | Buzzer | `Pin 6` | Yüksek Sıcaklık Uyarısı |

---

## 📡 Veri Formatları

### 1. LoRa Telemetri Paketi (Yer İstasyonu)
Yarış kuralları gereği kablosuz ortamda iletilen minimalist paket yapısı:
```text
Telemetri_Verisi:100,TIME:120450,SPD:42.5,Tmax:27.4,V:79.2
```
* `TIME`: Çalışma zamanı (ms)
* `SPD`: Araç Hızı (km/h)
* `Tmax`: En Yüksek Batarya Sıcaklığı (°C)
* `V`: Toplam Batarya Voltajı (V)

### 2. SD Kart CSV Formatı (Detaylı Sürüş Logu)
Tüm detayların kayıt altına alındığı CSV satır yapısı:
```text
zaman_ms;durum;hiz_kmh;T1_C;T2_C;T3_C;Tmax_C;V_bat_V;I_bat_A;SOC;kalan_enerji_Wh;izolasyon_kOhm;max_hucre_mV;min_hucre_mV
```
3. Kart tipini **Arduino Mega 2560** seçerek projeyi derleyin ve karta yükleyin.
