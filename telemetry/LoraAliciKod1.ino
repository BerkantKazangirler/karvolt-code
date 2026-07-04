#include <SoftwareSerial.h>

SoftwareSerial loraSerial(10, 11); 

#define M0 7
#define M1 6
#define LED_PIN 13 // Arduino üzerindeki dahili LED

void setup() {
  Serial.begin(9600);
  loraSerial.begin(9600);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(LED_PIN, OUTPUT); // LED pinini çıkış yap

  // NORMAL MOD (MODE 0)
  digitalWrite(M0, LOW); 
  digitalWrite(M1, LOW); 

  Serial.println("ALICI: Veri bekleniyor (Blink Aktif)...");
}

void loop() {
  if (loraSerial.available()) {
    // 1. ADIM: LED'İ YAK (Veri geldi!)
    digitalWrite(LED_PIN, HIGH);
    
    // 2. ADIM: VERİYİ OKU
    String gelen = loraSerial.readStringUntil('\n'); 
    Serial.println("GELEN: " + gelen);
    
    // 3. ADIM: KISA BİR BEKLEME VE LED'İ SÖNDÜR
    delay(2000); 
    digitalWrite(LED_PIN, LOW);
  }
}
